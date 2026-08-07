/*********************************************************************
 * @file        APP_SDM.c
 * @brief       Application diagnostic manager.
 * @note        Keeps runtime diagnostic monitoring separate from the
 *              persistent diagnostic history stored in FMK_NVM.
 *********************************************************************/

// ********************************************************************
// *                      Includes
// ********************************************************************
#include "./APP_SDM.h"
#include "APP_CFG/ConfigFiles/APPSDM_ConfigPrivate.h"
#include "APP_CTRL/APP_SYS/Src/APP_SYS.h"

#include "FMK_HAL/FMK_CPU/Src/FMK_CPU.h"
#include "FMK_HAL/FMK_NVM/Src/FMK_NVM.h"
#include "Library/SafeMem/SafeMem.h"

// ********************************************************************
// *                      Defines
// ********************************************************************

/**
 * @brief Persistent diagnostic record format marker.
 * @note The previous APP_SDM implementation also used 20-byte NVM objects.
 *       This marker allows the new layout to reject old 20-byte records
 *       instead of interpreting them with the new format.
 */
#define APPSDM_NVM_RECORD_FORMAT_MARKER       ((t_uint16)0xA5D2U)

#define APPSDM_NVM_EMPTY_ITEM_ID              ((t_uint16)0xFFFFU)

#define APPSDM_SECONDS_PER_MINUTE              ((t_uint32)60U)
#define APPSDM_SECONDS_PER_HOUR                ((t_uint32)3600U)
#define APPSDM_SECONDS_PER_DAY                 ((t_uint32)86400U)
#define APPSDM_MILLISECONDS_PER_SECOND         ((t_uint32)1000U)

// ********************************************************************
// *                      Types
// ********************************************************************

//-----------------------------ENUM TYPES-----------------------------//
typedef enum
{
    APPSDM_DIAG_ITEM_STATUS_OFF = 0x00, APPSDM_DIAG_ITEM_STATUS_DBC, APPSDM_DIAG_ITEM_STATUS_ON,

    APPSDM_DIAG_ITEM_STATUS_NB, } t_eAPPSDM_ItemState;

//-----------------------------STRUCT TYPES---------------------------//

/**
 * @brief Runtime-only state for one configured diagnostic item.
 * @note One runtime entry exists for every t_eAPPSDM_DiagnosticItem.
 *       It is intentionally independent from the ten persistent history slots.
 */
typedef struct
{
    t_eAPPSDM_ItemState         mngmtState_e;
    t_eAPPSDM_DiagnosticReport  reportState_e;
    t_uint32                    dbcCounter_u32;
    t_uint32                    lastReportTick_u32;
    t_uint32                    broadcastDelay_u32;
    t_uint32                    activeDurationTick_u32;
    t_uint16                    debugInfo1_u16;
    t_uint16                    debugInfo2_u16;
} t_sAPPSDM_RuntimeInfo;

/**
 * @brief Fixed 20-byte persistent diagnostic record.
 *
 * Layout intentionally remains exactly 20 bytes so the existing
 * FMKNVM_OBJECT_SDM_DIAG_ITEM_x object sizes do not need to change.
 *
 * firstOccurrence_s_u32 is the number of seconds since
 * 2000-01-01 00:00:00 according to the RTC.
 *
 * activeDuration_s_u32 is the cumulative qualified active duration
 * in seconds over all occurrences of this diagnostic item.
 */
typedef struct
{
    t_uint16 itemId_u16;
    t_uint16 formatMarker_u16;
    t_uint32 firstOccurrence_s_u32;
    t_uint32 occurrenceCount_u32;
    t_uint32 activeDuration_s_u32;
    t_uint16 debugInfo1_u16;
    t_uint16 debugInfo2_u16;
} t_sAPPSDM_NvmDiagRecord;

_Static_assert(sizeof(t_sAPPSDM_NvmDiagRecord) == 20U, "APP_SDM NVM diagnostic record must stay 20 bytes");

// ********************************************************************
// *                      Variables
// ********************************************************************

static t_eCyclicModState g_AppSdm_ModState_e = STATE_CYCLIC_CFG;

/**
 * @brief Runtime state indexed directly by diagnostic item identifier.
 */
static t_sAPPSDM_RuntimeInfo g_diagRuntime_as[APPSDM_DIAG_ITEM_NB];

/**
 * @brief Persistent history slots.
 * @note Slots remain occupied after a diagnostic becomes inactive until
 *       explicitly cleared or replaced by the oldest-entry policy.
 */
static t_sAPPSDM_NvmDiagRecord g_diagHistory_as[APPSDM_MAX_DIAG_ITEM_MONITORING];

/// @brief flag Request OPe Nvm Mng
static t_bool g_RqstNvmOpe_b = (t_bool)FALSE;

/// @brief How many diag item valid in Nvm
static t_uint8 g_diagHistoryCnt_u8 = (t_uint8)0;

///@brief managmeent of cylic nvm mng
static t_uint32 g_lastNvmSyncTick_u32 = 0U;

static t_cbAPPSDM_DiagEventBroadcast * g_UserCallback_pcb = (t_cbAPPSDM_DiagEventBroadcast *)NULL_FUNCTION;

// ********************************************************************
// *                      Local functions - Prototypes
// ********************************************************************

/**
 * @brief Reset one diagnostic runtime information structure.
 *
 * @details
 * Resets all runtime-only information associated with a diagnostic item,
 * including its management state, debounce counter, report state,
 * timestamps and debug information.
 *
 * @param[in,out] f_Runtime_ps
 * Pointer to the runtime diagnostic information structure to reset.
 *
 * @return None.
 */
static void s_APPSDM_ResetRuntimeInfo(t_sAPPSDM_RuntimeInfo * f_Runtime_ps);

/**
 * @brief Reset one persistent diagnostic history record.
 *
 * @details
 * Initializes the supplied NVM diagnostic record to its empty state.
 * The diagnostic item identifier is invalidated and all persistent
 * diagnostic information is reset.
 *
 * @param[in,out] f_Record_ps
 * Pointer to the persistent diagnostic record to reset.
 *
 * @return None.
 */
static void s_APPSDM_ResetHistoryRecord(t_sAPPSDM_NvmDiagRecord * f_Record_ps);

/**
 * @brief Check whether a persistent diagnostic history record is valid.
 *
 * @details
 * Verifies the persistent record format marker, diagnostic item identifier
 * and occurrence counter in order to determine whether the record contains
 * a valid stored diagnostic.
 *
 * @param[in] f_Record_ps
 * Pointer to the persistent diagnostic record to validate.
 *
 * @retval TRUE
 * The record contains a valid diagnostic history entry.
 *
 * @retval FALSE
 * The record is invalid, empty or corrupted.
 */
static t_bool s_APPSDM_IsHistoryRecordValid(const t_sAPPSDM_NvmDiagRecord * f_Record_ps);

/**
 * @brief Find the persistent history record associated with a diagnostic item.
 *
 * @details
 * Searches all persistent diagnostic history slots for a valid record whose
 * diagnostic item identifier matches the requested diagnostic item.
 *
 * @param[in] f_Item_e
 * Diagnostic item identifier to search for.
 *
 * @param[out] f_SlotIdx_pu8
 * Optional pointer used to return the corresponding history slot index.
 * This pointer may be NULL when the slot index is not required.
 *
 * @return
 * Pointer to the matching persistent diagnostic record when found.
 *
 * @retval NULL
 * No persistent record exists for the requested diagnostic item.
 */
static t_sAPPSDM_NvmDiagRecord * s_APPSDM_FindHistoryRecord(   t_eAPPSDM_DiagnosticItem f_Item_e,
                                                               t_uint8 * f_SlotIdx_pu8);

/**
 * @brief Select a persistent history slot for a new diagnostic item.
 *
 * @details
 * Searches first for an unused persistent diagnostic slot.
 *
 * If all history slots are occupied, the oldest inactive diagnostic record
 * is selected for replacement.
 *
 * If every stored diagnostic is currently active, the oldest record is
 * selected according to its first occurrence timestamp.
 *
 * @return
 * Index of the selected diagnostic history slot.
 *
 * @retval APPSDM_MAX_DIAG_ITEM_MONITORING
 * No valid history slot could be selected.
 */
static t_uint8 s_APPSDM_SelectHistorySlot(void);

/**
 * @brief Register one newly qualified diagnostic occurrence.
 *
 * @details
 * Updates the persistent diagnostic history when a diagnostic becomes
 * qualified active.
 *
 * If the diagnostic item already exists in the history, its occurrence
 * counter is incremented and its debug information is updated.
 *
 * If the diagnostic item is not yet present, a new persistent record is
 * created. When all history slots are already used, the slot selected by
 * s_APPSDM_SelectHistorySlot() is replaced.
 *
 * The first occurrence timestamp is kept unchanged for an already existing
 * diagnostic record.
 *
 * @param[in] f_Item_e
 * Diagnostic item associated with the new occurrence.
 *
 * @param[in] f_Runtime_ps
 * Pointer to the current runtime information of the diagnostic item.
 *
 * @retval RC_OK
 * The diagnostic occurrence was successfully registered.
 *
 * @retval RC_ERROR_PARAM_INVALID
 * The diagnostic item identifier or runtime information is invalid.
 *
 * @retval RC_ERROR_WRONG_RESULT
 * No persistent diagnostic history slot could be selected.
 */
static t_eReturnCode s_APPSDM_RegisterOccurrence(   t_eAPPSDM_DiagnosticItem f_Item_e,
                                                    const t_sAPPSDM_RuntimeInfo * f_Runtime_ps);

/**
 * @brief Update the cumulative active duration of one diagnostic item.
 *
 * @details
 * Computes the elapsed time since the previous active-duration update and
 * accumulates complete elapsed seconds into the persistent diagnostic
 * history record.
 *
 * The remaining sub-second duration is preserved through the runtime
 * timestamp for the next update.
 *
 * @param[in] f_Item_e
 * Diagnostic item whose active duration must be updated.
 *
 * @param[in,out] f_Runtime_ps
 * Pointer to the diagnostic runtime information.
 *
 * @param[in] f_CurrentTick_u32
 * Current system tick expressed in milliseconds.
 *
 * @return None.
 */
static void s_APPSDM_UpdateActiveDuration(   t_eAPPSDM_DiagnosticItem f_Item_e,
                                             t_sAPPSDM_RuntimeInfo * f_Runtime_ps,
                                             t_uint32 f_CurrentTick_u32);

/**
 * @brief Manage the runtime state of one diagnostic item.
 *
 * @details
 * Applies the diagnostic management state machine for the requested item.
 *
 * The function handles:
 * - diagnostic debounce;
 * - transition from debounce state to active state;
 * - persistent occurrence registration;
 * - cumulative active-duration update;
 * - diagnostic strategy inhibition and de-inhibition;
 * - user notification;
 * - transition back to inactive state.
 *
 * @param[in] f_Item_e
 * Diagnostic item currently being managed.
 *
 * @param[in,out] f_Runtime_ps
 * Pointer to the diagnostic runtime information.
 *
 * @param[in] f_ItemCfg_ps
 * Pointer to the diagnostic item configuration.
 *
 * @retval RC_OK
 * Diagnostic management completed successfully.
 *
 * @retval RC_ERROR_PTR_NULL
 * One of the supplied pointers is NULL.
 *
 * @retval RC_ERROR_PARAM_INVALID
 * The diagnostic item identifier or diagnostic strategy is invalid.
 *
 * @return
 * Other errors may be propagated by the diagnostic strategy or persistent
 * diagnostic management functions.
 */
static t_eReturnCode s_APPSDM_DiagnosticMngmt(   t_eAPPSDM_DiagnosticItem f_Item_e,
                                                 t_sAPPSDM_RuntimeInfo * f_Runtime_ps,
                                                 const t_sAPPSM_DiagItemCfg * f_ItemCfg_ps);

/**
 * @brief Apply or release a diagnostic strategy.
 *
 * @details
 * Calls the application diagnostic strategy callback associated with the
 * requested diagnostic strategy.
 *
 * No callback is executed when APPSDM_DIAG_STRAT_NONE is requested.
 *
 * @param[in] f_diagStrat_e
 * Diagnostic strategy to apply.
 *
 * @param[in] f_stratOpe_e
 * Operation to perform on the diagnostic strategy.
 *
 * @retval RC_OK
 * The diagnostic strategy operation was successfully processed.
 *
 * @retval RC_ERROR_PARAM_INVALID
 * The diagnostic strategy identifier is invalid.
 */
static t_eReturnCode s_APPSDM_DiagStratMngmt(   t_eAPPSDM_DiagnosticStrat f_diagStrat_e,
                                                t_eAPPSDM_DiagStratOpe f_stratOpe_e);

/**
 * @brief Restore the persistent diagnostic history during module configuration.
 *
 * @details
 * Reads each APP_SDM diagnostic object from FMK_NVM and restores valid
 * diagnostic history entries into RAM.
 *
 * Empty, unavailable, corrupted or incompatible records are initialized
 * with the current APP_SDM persistent record format.
 *
 * @retval RC_OK
 * Diagnostic history restoration completed successfully.
 *
 * @return
 * FMK_NVM or memory-copy errors may be propagated to the caller.
 */
static t_eReturnCode s_APPSDM_CfgSts(void);

/**
 * @brief Perform APP_SDM operational processing.
 *
 * @details
 * Executes the diagnostic runtime management and, when required,
 * synchronizes modified persistent diagnostic history records with FMK_NVM.
 *
 * @retval RC_OK
 * Operational processing completed successfully.
 *
 * @return
 * Errors from diagnostic management or NVM management may be propagated
 * to the caller.
 */
static t_eReturnCode s_APPSDM_Operational(void);

/**
 * @brief Process all currently monitored diagnostic items.
 *
 * @details
 * Iterates over the complete diagnostic item runtime table and executes
 * s_APPSDM_DiagnosticMngmt() for every diagnostic item that is currently
 * in debounce or active state.
 *
 * @retval RC_OK
 * All diagnostic items were processed successfully.
 *
 * @return
 * Any error returned by the diagnostic management state machine.
 */
static t_eReturnCode s_APPSDM_Ope_DiagMngmt(void);

/**
 * @brief Synchronize the diagnostic history RAM cache with FMK_NVM.
 *
 * @details
 * Writes each persistent diagnostic history record into its associated
 * FMK_NVM logical object.
 *
 * FMK_NVM performs byte comparison internally and therefore unchanged
 * diagnostic records do not generate a new physical NVM update.
 *
 * @retval RC_OK
 * Diagnostic history synchronization completed successfully.
 *
 * @return
 * Any error returned by FMKNVM_SetObject().
 */
static t_eReturnCode s_APPSDM_Ope_NvmMngmt(void);

/**
 * @brief Determine whether a year is a Gregorian leap year.
 *
 * @param[in] f_Year_u16
 * Full calendar year, for example 2026.
 *
 * @retval TRUE
 * The supplied year is a leap year.
 *
 * @retval FALSE
 * The supplied year is not a leap year.
 */
static t_bool s_APPSDM_IsLeapYear(t_uint16 f_Year_u16);

/**
 * @brief Get the number of days in a calendar month.
 *
 * @details
 * Returns the number of days for the requested month and takes leap years
 * into account for February.
 *
 * @param[in] f_Year_u16
 * Full calendar year.
 *
 * @param[in] f_Month_u8
 * Calendar month in the range [1, 12].
 *
 * @return
 * Number of days in the requested month.
 *
 * @retval 0
 * The supplied month is invalid.
 */
static t_uint8 s_APPSDM_GetDaysInMonth(   t_uint16 f_Year_u16,
                                          t_uint8 f_Month_u8);

/**
 * @brief Convert an RTC date and time into an APP_SDM timestamp.
 *
 * @details
 * Converts the supplied calendar date and time into the number of elapsed
 * seconds since the APP_SDM timestamp epoch:
 *
 * 2000-01-01 00:00:00.
 *
 * The resulting timestamp is used as the persistent representation of the
 * diagnostic first occurrence date.
 *
 * @param[in] f_DateTime_ps
 * Pointer to the RTC date and time to convert.
 *
 * @param[out] f_Timestamp_pu32
 * Pointer receiving the timestamp expressed in seconds since
 * 2000-01-01 00:00:00.
 *
 * @retval RC_OK
 * The conversion completed successfully.
 *
 * @retval RC_ERROR_PTR_NULL
 * One of the supplied pointers is NULL.
 *
 * @retval RC_ERROR_PARAM_INVALID
 * The supplied calendar date or time is invalid.
 */
static t_eReturnCode s_APPSDM_DateTimeToTimestamp(   const t_sFMKCPU_DateTime * f_DateTime_ps,
                                                     t_uint32 * f_Timestamp_pu32);

/**
 * @brief Get the current RTC date and time as an APP_SDM timestamp.
 *
 * @details
 * Checks whether the RTC date and time are valid and, when valid, reads the
 * current RTC value through FMK_CPU before converting it into the APP_SDM
 * persistent timestamp format.
 *
 * A timestamp value of zero is used when no valid RTC date has been
 * configured.
 *
 * @param[out] f_Timestamp_pu32
 * Pointer receiving the timestamp expressed in seconds since
 * 2000-01-01 00:00:00.
 *
 * @retval RC_OK
 * The timestamp was successfully retrieved or the RTC was not yet valid.
 *
 * @retval RC_ERROR_PTR_NULL
 * The destination pointer is NULL.
 *
 * @return
 * Errors from FMKCPU_IsDateTimeValid(), FMKCPU_GetDateTime() or the
 * date/time conversion may be propagated.
 */
static t_eReturnCode s_APPSDM_GetCurrentTimestamp(t_uint32 * f_Timestamp_pu32);

/**
 * @brief Convert an APP_SDM timestamp into a calendar date and time.
 *
 * @details
 * Converts a number of elapsed seconds since
 * 2000-01-01 00:00:00 into the public APP_SDM date/time representation.
 *
 * This function is mainly used when diagnostic history information is
 * requested by a client such as the HMI.
 *
 * @param[in] f_Timestamp_u32
 * Timestamp expressed in seconds since 2000-01-01 00:00:00.
 *
 * @param[out] f_DateTime_ps
 * Pointer receiving the converted calendar date and time.
 *
 * @return None.
 */
static void s_APPSDM_TimestampToDateTime(   t_uint32 f_Timestamp_u32,
                                            t_sAPPSDM_DateTime * f_DateTime_ps);
// ********************************************************************
// *                      Public functions - Implementation
// ********************************************************************

/*********************************
 * APPSDM_Init
 *********************************/
t_eReturnCode APPSDM_Init(void)
{
    t_uint16 idxItem_u16;

    for(idxItem_u16 = 0U; idxItem_u16 < (t_uint16)APPSDM_DIAG_ITEM_NB; idxItem_u16++)
    {
        s_APPSDM_ResetRuntimeInfo(&g_diagRuntime_as[idxItem_u16]);
    }

    for(idxItem_u16 = 0U; idxItem_u16 < (t_uint16)APPSDM_MAX_DIAG_ITEM_MONITORING; idxItem_u16++)
    {
        s_APPSDM_ResetHistoryRecord(&g_diagHistory_as[idxItem_u16]);
    }

    g_diagHistoryCnt_u8 = (t_uint8)0U;
    g_RqstNvmOpe_b = (t_bool)FALSE;

    return RC_OK;
}

/*********************************
 * APPSDM_Cyclic
 *********************************/
t_eReturnCode APPSDM_Cyclic(void)
{
    t_eReturnCode Ret_e = RC_OK;

    switch(g_AppSdm_ModState_e)
    {
        case STATE_CYCLIC_CFG:
        {
            Ret_e = s_APPSDM_CfgSts();

            if(Ret_e == RC_OK)
            {
                g_AppSdm_ModState_e = STATE_CYCLIC_PREOPE;
            }
            else if(Ret_e < RC_OK)
            {
                g_AppSdm_ModState_e = STATE_CYCLIC_ERROR;
            }
            break;
        }

        case STATE_CYCLIC_PREOPE:
        {
            g_AppSdm_ModState_e = STATE_CYCLIC_OPE;
            break;
        }

        case STATE_CYCLIC_OPE:
        {
            Ret_e = s_APPSDM_Operational();

            if(Ret_e < RC_OK)
            {
                ASSERT((t_sint32)Ret_e);
                g_AppSdm_ModState_e = STATE_CYCLIC_ERROR;
            }
            break;
        }

        case STATE_CYCLIC_ERROR:
        {
            break;
        }

        case STATE_CYCLIC_BUSY: default:
        {
            Ret_e = RC_OK;
            break;
        }
    }

    return Ret_e;
}

/*********************************
 * APPSDM_GetState
 *********************************/
t_eReturnCode APPSDM_GetState(t_eCyclicModState * f_State_pe)
{
    t_eReturnCode Ret_e = RC_OK;

    if(f_State_pe == (t_eCyclicModState *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        *f_State_pe = g_AppSdm_ModState_e;
    }

    return Ret_e;
}

/*********************************
 * APPSDM_SetState
 *********************************/
t_eReturnCode APPSDM_SetState(t_eCyclicModState f_State_e)
{
    g_AppSdm_ModState_e = f_State_e;
    return RC_OK;
}

/*********************************
 * APPSDM_ReportDiagEvnt
 *********************************/
void APPSDM_ReportDiagEvnt(   t_eAPPSDM_DiagnosticItem f_item_e,
                              t_eAPPSDM_DiagnosticReport f_reportState_e,
                              t_uint16 f_debugInfo1_u16,
                              t_uint16 f_debugInfo2_u16)
{
    t_sAPPSDM_RuntimeInfo * Runtime_ps;
    t_uint32 currentTick_u32;

    if((f_item_e >= APPSDM_DIAG_ITEM_NB) || (f_reportState_e >= APPSDM_DIAG_ITEM_STATE_NB))
    {
        ASSERT((t_sint32)0);
    }
    else if((APPSDM_DIAG_MNGMT_STATUS == (t_bool)TRUE)
    &&      (g_AppSdm_ModState_e == STATE_CYCLIC_OPE))
    {
        Runtime_ps = &g_diagRuntime_as[f_item_e];
        FMKCPU_GetTick(&currentTick_u32);

        if(f_reportState_e == APPSDM_DIAG_ITEM_REPORT_FAIL)
        {
            Runtime_ps->debugInfo1_u16 = f_debugInfo1_u16;
            Runtime_ps->debugInfo2_u16 = f_debugInfo2_u16;
            Runtime_ps->reportState_e = APPSDM_DIAG_ITEM_REPORT_FAIL;
            Runtime_ps->lastReportTick_u32 = currentTick_u32;

            if(Runtime_ps->mngmtState_e == APPSDM_DIAG_ITEM_STATUS_OFF)
            {
                Runtime_ps->dbcCounter_u32 = 1U;
                Runtime_ps->broadcastDelay_u32 = currentTick_u32;
                Runtime_ps->mngmtState_e = APPSDM_DIAG_ITEM_STATUS_DBC;
            }
            else if(Runtime_ps->mngmtState_e == APPSDM_DIAG_ITEM_STATUS_DBC)
            {
                if(Runtime_ps->dbcCounter_u32 < 0xFFFFFFFFUL)
                {
                    Runtime_ps->dbcCounter_u32++;
                }
            }

            /*
             * Preserve the previous behavior for diagnostics configured
             * without debounce: qualify them immediately.
             */
            if((Runtime_ps->mngmtState_e == APPSDM_DIAG_ITEM_STATUS_DBC) 
            && (c_AppSdm_DiagItemCfg_as[f_item_e].DebuncCnt_u16 == 0U))
            {
                (void)s_APPSDM_DiagnosticMngmt(f_item_e, Runtime_ps, &c_AppSdm_DiagItemCfg_as[f_item_e]);
            }
        }
        else
        {
            /*
             * PASS does not erase history. It only terminates the current
             * runtime occurrence. The NVM entry remains present.
             */
            Runtime_ps->reportState_e = APPSDM_DIAG_ITEM_REPORT_PASS;
            Runtime_ps->lastReportTick_u32 = currentTick_u32;
        }
    }

    return;
}

/*********************************
 * APPSDM_GetDiagStatus
 *********************************/
t_eReturnCode APPSDM_GetDiagStatus(   t_eAPPSDM_DiagnosticItem f_item_e,
                                      t_eAPPSDM_DiagnosticReport * f_reportState_pe)
{
    t_eReturnCode Ret_e = RC_OK;

    if(f_item_e >= APPSDM_DIAG_ITEM_NB)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else if(f_reportState_pe == (t_eAPPSDM_DiagnosticReport *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        const t_sAPPSDM_RuntimeInfo * Runtime_ps = &g_diagRuntime_as[f_item_e];

        if(Runtime_ps->mngmtState_e == APPSDM_DIAG_ITEM_STATUS_OFF)
        {
            *f_reportState_pe = APPSDM_DIAG_ITEM_REPORT_PASS;
        }
        else
        {
            *f_reportState_pe = Runtime_ps->reportState_e;
        }
    }

    return Ret_e;
}

/*********************************
 * APPSDM_GetDiagHistoryCount
 *********************************/
t_eReturnCode APPSDM_GetDiagHistoryCount(t_uint8 * f_Count_pu8)
{
    t_eReturnCode Ret_e = RC_OK;

    if(f_Count_pu8 == (t_uint8 *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        *f_Count_pu8 = g_diagHistoryCnt_u8;
    }

    return Ret_e;
}

/*********************************
 * APPSDM_GetDiagHistoryItem
 *********************************/
t_eReturnCode APPSDM_GetDiagHistoryItem(   t_uint8 f_SlotIdx_u8,
                                           t_sAPPSDM_DiagHistoryItem * f_ItemInfo_ps,
                                           t_bool * f_IsAvailable_pb)
{
    t_eReturnCode Ret_e = RC_OK;

    if(f_SlotIdx_u8 >= APPSDM_MAX_DIAG_ITEM_MONITORING)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else if((f_ItemInfo_ps == (t_sAPPSDM_DiagHistoryItem *)NULL) || (f_IsAvailable_pb == (t_bool *)NULL))
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else if(g_AppSdm_ModState_e != STATE_CYCLIC_OPE)
    {
        Ret_e = RC_WARNING_BUSY;
    }
    else
    {
        const t_sAPPSDM_NvmDiagRecord * Record_ps = &g_diagHistory_as[f_SlotIdx_u8];

        *f_IsAvailable_pb =
            s_APPSDM_IsHistoryRecordValid(Record_ps);

        f_ItemInfo_ps->itemId_e = APPSDM_DIAG_ITEM_NB;
        f_ItemInfo_ps->occurrenceCount_u32 = 0U;
        f_ItemInfo_ps->activeDuration_s_u32 = 0U;
        f_ItemInfo_ps->debugInfo1_u16 = 0U;
        f_ItemInfo_ps->debugInfo2_u16 = 0U;
        f_ItemInfo_ps->isActive_b = (t_bool)FALSE;
        f_ItemInfo_ps->isFirstOccurrenceDateValid_b = (t_bool)FALSE;

        f_ItemInfo_ps->firstOccurrence_s.year_u16 = 0U;
        f_ItemInfo_ps->firstOccurrence_s.month_u8 = 0U;
        f_ItemInfo_ps->firstOccurrence_s.day_u8 = 0U;
        f_ItemInfo_ps->firstOccurrence_s.hour_u8 = 0U;
        f_ItemInfo_ps->firstOccurrence_s.minute_u8 = 0U;
        f_ItemInfo_ps->firstOccurrence_s.second_u8 = 0U;

        if(*f_IsAvailable_pb == (t_bool)TRUE)
        {
            t_eAPPSDM_DiagnosticItem Item_e = (t_eAPPSDM_DiagnosticItem)Record_ps->itemId_u16;

            f_ItemInfo_ps->itemId_e = Item_e;
            f_ItemInfo_ps->occurrenceCount_u32 = Record_ps->occurrenceCount_u32;
            f_ItemInfo_ps->activeDuration_s_u32 = Record_ps->activeDuration_s_u32;
            f_ItemInfo_ps->debugInfo1_u16 = Record_ps->debugInfo1_u16;
            f_ItemInfo_ps->debugInfo2_u16 = Record_ps->debugInfo2_u16;

            if(Record_ps->firstOccurrence_s_u32 != 0U)
            {
                s_APPSDM_TimestampToDateTime(Record_ps->firstOccurrence_s_u32, &f_ItemInfo_ps->firstOccurrence_s);

                f_ItemInfo_ps->isFirstOccurrenceDateValid_b = (t_bool)TRUE;
            }

            if(Item_e < APPSDM_DIAG_ITEM_NB)
            {
                const t_sAPPSDM_RuntimeInfo * Runtime_ps = &g_diagRuntime_as[Item_e];

                f_ItemInfo_ps->isActive_b = (Runtime_ps->mngmtState_e == APPSDM_DIAG_ITEM_STATUS_ON) ? (t_bool)TRUE : (t_bool)FALSE;

                /*
                 * Include the sub-second runtime interval not yet folded into
                 * the cumulative whole-second counter.
                 */
                if(f_ItemInfo_ps->isActive_b == (t_bool)TRUE)
                {
                    t_uint32 currentTick_u32;
                    t_uint32 elapsedMs_u32;

                    FMKCPU_GetTick(&currentTick_u32);
                    elapsedMs_u32 = currentTick_u32 - Runtime_ps->activeDurationTick_u32;

                    f_ItemInfo_ps->activeDuration_s_u32 += elapsedMs_u32 / APPSDM_MILLISECONDS_PER_SECOND;
                }
            }
        }
    }

    return Ret_e;
}

/*********************************
 * APPSDM_ClearDiagHistory
 *********************************/
t_eReturnCode APPSDM_ClearDiagHistory(void)
{
    t_eReturnCode Ret_e = RC_OK;
    t_uint16 idx_u16;
    t_uint32 currentTick_u32;

    /*
     * Clearing history must not stop an active diagnostic strategy.
     * Active faults are recreated as a fresh occurrence after the clear.
     */
    for(idx_u16 = 0U; idx_u16 < (t_uint16)APPSDM_MAX_DIAG_ITEM_MONITORING; idx_u16++)
    {
        s_APPSDM_ResetHistoryRecord(&g_diagHistory_as[idx_u16]);
    }

    g_diagHistoryCnt_u8 = 0U;

    FMKCPU_GetTick(&currentTick_u32);

    for(idx_u16 = 0U; (idx_u16 < (t_uint16)APPSDM_DIAG_ITEM_NB) && (Ret_e == RC_OK); idx_u16++)
    {
        t_sAPPSDM_RuntimeInfo * Runtime_ps = &g_diagRuntime_as[idx_u16];

        if(Runtime_ps->mngmtState_e == APPSDM_DIAG_ITEM_STATUS_ON)
        {
            Runtime_ps->activeDurationTick_u32 = currentTick_u32;

            Ret_e = s_APPSDM_RegisterOccurrence((t_eAPPSDM_DiagnosticItem)idx_u16, Runtime_ps);
        }
    }

    if(Ret_e == RC_OK)
    {
        g_RqstNvmOpe_b = (t_bool)TRUE;
    }

    return Ret_e;
}

/*********************************
 * APPSDM_AddCallbackEvnt
 *********************************/
t_eReturnCode APPSDM_AddCallbackEvnt(t_cbAPPSDM_DiagEventBroadcast * f_evntCallback_pcb)
{
    t_eReturnCode Ret_e = RC_OK;

    if(f_evntCallback_pcb == (t_cbAPPSDM_DiagEventBroadcast *)NULL_FUNCTION)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else if(g_UserCallback_pcb != (t_cbAPPSDM_DiagEventBroadcast *)NULL_FUNCTION)
    {
        Ret_e = RC_ERROR_ALREADY_CONFIGURED;
    }
    else
    {
        g_UserCallback_pcb = f_evntCallback_pcb;
    }

    return Ret_e;
}

// ********************************************************************
// *                      Local functions - Implementation
// ********************************************************************

/*********************************
 * s_APPSDM_ResetRuntimeInfo
 *********************************/
static void s_APPSDM_ResetRuntimeInfo(t_sAPPSDM_RuntimeInfo * f_Runtime_ps)
{
    if(f_Runtime_ps != (t_sAPPSDM_RuntimeInfo *)NULL)
    {
        f_Runtime_ps->mngmtState_e = APPSDM_DIAG_ITEM_STATUS_OFF;
        f_Runtime_ps->reportState_e = APPSDM_DIAG_ITEM_REPORT_PASS;
        f_Runtime_ps->dbcCounter_u32 = 0U;
        f_Runtime_ps->lastReportTick_u32 = 0U;
        f_Runtime_ps->broadcastDelay_u32 = 0U;
        f_Runtime_ps->activeDurationTick_u32 = 0U;
        f_Runtime_ps->debugInfo1_u16 = 0U;
        f_Runtime_ps->debugInfo2_u16 = 0U;
    }
}

/*********************************
 * s_APPSDM_ResetHistoryRecord
 *********************************/
static void s_APPSDM_ResetHistoryRecord(t_sAPPSDM_NvmDiagRecord * f_Record_ps)
{
    if(f_Record_ps != (t_sAPPSDM_NvmDiagRecord *)NULL)
    {
        f_Record_ps->itemId_u16 = APPSDM_NVM_EMPTY_ITEM_ID;
        f_Record_ps->formatMarker_u16 = APPSDM_NVM_RECORD_FORMAT_MARKER;
        f_Record_ps->firstOccurrence_s_u32 = 0U;
        f_Record_ps->occurrenceCount_u32 = 0U;
        f_Record_ps->activeDuration_s_u32 = 0U;
        f_Record_ps->debugInfo1_u16 = 0U;
        f_Record_ps->debugInfo2_u16 = 0U;
    }
}

/*********************************
 * s_APPSDM_IsHistoryRecordValid
 *********************************/
static t_bool s_APPSDM_IsHistoryRecordValid(const t_sAPPSDM_NvmDiagRecord * f_Record_ps)
{
    t_bool IsValid_b = (t_bool)FALSE;

    if((f_Record_ps != (const t_sAPPSDM_NvmDiagRecord *)NULL) && (f_Record_ps->formatMarker_u16 == APPSDM_NVM_RECORD_FORMAT_MARKER) &&
       (f_Record_ps->itemId_u16 < (t_uint16)APPSDM_DIAG_ITEM_NB) && (f_Record_ps->occurrenceCount_u32 > 0U))
    {
        IsValid_b = (t_bool)TRUE;
    }

    return IsValid_b;
}

/*********************************
 * s_APPSDM_FindHistoryRecord
 *********************************/
static t_sAPPSDM_NvmDiagRecord * s_APPSDM_FindHistoryRecord(   t_eAPPSDM_DiagnosticItem f_Item_e,
                                                               t_uint8 * f_SlotIdx_pu8)
{
    t_sAPPSDM_NvmDiagRecord * Record_ps = (t_sAPPSDM_NvmDiagRecord *)NULL;
    t_uint8 idx_u8;

    for(idx_u8 = 0U; idx_u8 < APPSDM_MAX_DIAG_ITEM_MONITORING; idx_u8++)
    {
        if((s_APPSDM_IsHistoryRecordValid(&g_diagHistory_as[idx_u8]) == (t_bool)TRUE) && (g_diagHistory_as[idx_u8].itemId_u16 ==
            (t_uint16)f_Item_e))
        {
            Record_ps = &g_diagHistory_as[idx_u8];

            if(f_SlotIdx_pu8 != (t_uint8 *)NULL)
            {
                *f_SlotIdx_pu8 = idx_u8;
            }

            break;
        }
    }

    return Record_ps;
}

/*********************************
 * s_APPSDM_SelectHistorySlot
 *********************************/
static t_uint8 s_APPSDM_SelectHistorySlot(void)
{
    t_uint8 idx_u8;
    t_uint8 selectedIdx_u8 = APPSDM_MAX_DIAG_ITEM_MONITORING;
    t_uint32 oldestTimestamp_u32 = 0xFFFFFFFFUL;

    /* First use a genuinely free slot. */
    for(idx_u8 = 0U; idx_u8 < APPSDM_MAX_DIAG_ITEM_MONITORING; idx_u8++)
    {
        if(s_APPSDM_IsHistoryRecordValid(&g_diagHistory_as[idx_u8]) == (t_bool)FALSE)
        {
            selectedIdx_u8 = idx_u8;
            break;
        }
    }

    /*
     * When full, replace the oldest inactive historical diagnostic first.
     * This keeps currently active diagnostics in the history whenever
     * possible.
     */
    if(selectedIdx_u8 >= APPSDM_MAX_DIAG_ITEM_MONITORING)
    {
        for(idx_u8 = 0U; idx_u8 < APPSDM_MAX_DIAG_ITEM_MONITORING; idx_u8++)
        {
            t_eAPPSDM_DiagnosticItem Item_e = (t_eAPPSDM_DiagnosticItem)
                g_diagHistory_as[idx_u8].itemId_u16;

            if((Item_e < APPSDM_DIAG_ITEM_NB) && (g_diagRuntime_as[Item_e].mngmtState_e == APPSDM_DIAG_ITEM_STATUS_OFF))
            {
                t_uint32 timestamp_u32 = g_diagHistory_as[idx_u8].firstOccurrence_s_u32;

                /*
                 * A zero timestamp means the RTC was not valid when the
                 * first occurrence was stored. Treat it as the oldest entry.
                 */
                if((timestamp_u32 == 0U) || (timestamp_u32 < oldestTimestamp_u32))
                {
                    selectedIdx_u8 = idx_u8;
                    oldestTimestamp_u32 = timestamp_u32;

                    if(timestamp_u32 == 0U)
                    {
                        break;
                    }
                }
            }
        }
    }

    /*
     * Extreme case: more than ten different diagnostics are active at the
     * same time. Apply the requested oldest-entry replacement policy even
     * though all stored entries are active. Runtime monitoring is separate,
     * therefore replacing the history entry does not stop the diagnostic.
     */
    if(selectedIdx_u8 >= APPSDM_MAX_DIAG_ITEM_MONITORING)
    {
        oldestTimestamp_u32 = 0xFFFFFFFFUL;

        for(idx_u8 = 0U; idx_u8 < APPSDM_MAX_DIAG_ITEM_MONITORING; idx_u8++)
        {
            t_uint32 timestamp_u32 = g_diagHistory_as[idx_u8].firstOccurrence_s_u32;

            if((timestamp_u32 == 0U) || (timestamp_u32 < oldestTimestamp_u32))
            {
                selectedIdx_u8 = idx_u8;
                oldestTimestamp_u32 = timestamp_u32;

                if(timestamp_u32 == 0U)
                {
                    break;
                }
            }
        }
    }

    return selectedIdx_u8;
}

/*********************************
 * s_APPSDM_RegisterOccurrence
 *********************************/
static t_eReturnCode s_APPSDM_RegisterOccurrence(   t_eAPPSDM_DiagnosticItem f_Item_e,
                                                    const t_sAPPSDM_RuntimeInfo * f_Runtime_ps)
{
    t_eReturnCode Ret_e = RC_OK;
    t_sAPPSDM_NvmDiagRecord * Record_ps;
    t_uint8 slotIdx_u8 = APPSDM_MAX_DIAG_ITEM_MONITORING;

    if((f_Item_e >= APPSDM_DIAG_ITEM_NB) || (f_Runtime_ps == (const t_sAPPSDM_RuntimeInfo *)NULL))
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else
    {
        Record_ps = s_APPSDM_FindHistoryRecord(f_Item_e, &slotIdx_u8);

        if(Record_ps == (t_sAPPSDM_NvmDiagRecord *)NULL)
        {
            t_uint32 timestamp_u32 = 0U;

            slotIdx_u8 = s_APPSDM_SelectHistorySlot();

            if(slotIdx_u8 >= APPSDM_MAX_DIAG_ITEM_MONITORING)
            {
                Ret_e = RC_ERROR_WRONG_RESULT;
            }
            else
            {
                Record_ps = &g_diagHistory_as[slotIdx_u8];

                if(s_APPSDM_IsHistoryRecordValid(Record_ps) == (t_bool)FALSE)
                {
                    if(g_diagHistoryCnt_u8 < APPSDM_MAX_DIAG_ITEM_MONITORING)
                    {
                        g_diagHistoryCnt_u8++;
                    }
                }

                s_APPSDM_ResetHistoryRecord(Record_ps);

                (void)s_APPSDM_GetCurrentTimestamp(&timestamp_u32);

                Record_ps->itemId_u16 = (t_uint16)f_Item_e;
                Record_ps->firstOccurrence_s_u32 = timestamp_u32;
                Record_ps->occurrenceCount_u32 = 1U;
                Record_ps->activeDuration_s_u32 = 0U;
                Record_ps->debugInfo1_u16 = f_Runtime_ps->debugInfo1_u16;
                Record_ps->debugInfo2_u16 = f_Runtime_ps->debugInfo2_u16;
            }
        }
        else
        {
            if(Record_ps->occurrenceCount_u32 < 0xFFFFFFFFUL)
            {
                Record_ps->occurrenceCount_u32++;
            }

            /*
             * Keep the first-occurrence date, but refresh the freeze-frame
             * debug information on every newly qualified occurrence.
             */
            Record_ps->debugInfo1_u16 = f_Runtime_ps->debugInfo1_u16;
            Record_ps->debugInfo2_u16 = f_Runtime_ps->debugInfo2_u16;
        }

        if(Ret_e == RC_OK)
        {
            g_RqstNvmOpe_b = (t_bool)TRUE;
        }
    }

    return Ret_e;
}

/*********************************
 * s_APPSDM_UpdateActiveDuration
 *********************************/
static void s_APPSDM_UpdateActiveDuration(   t_eAPPSDM_DiagnosticItem f_Item_e,
                                             t_sAPPSDM_RuntimeInfo * f_Runtime_ps,
                                             t_uint32 f_CurrentTick_u32)
{
    t_sAPPSDM_NvmDiagRecord * Record_ps;
    t_uint32 elapsedMs_u32;
    t_uint32 elapsedSeconds_u32;

    if((f_Item_e < APPSDM_DIAG_ITEM_NB) 
    && (f_Runtime_ps != (t_sAPPSDM_RuntimeInfo *)NULL))
    {

        elapsedMs_u32 = f_CurrentTick_u32 - f_Runtime_ps->activeDurationTick_u32;

        elapsedSeconds_u32 = elapsedMs_u32 / APPSDM_MILLISECONDS_PER_SECOND;

        if(elapsedSeconds_u32 > 0U)
        {

            Record_ps = s_APPSDM_FindHistoryRecord(f_Item_e, (t_uint8 *)NULL);

            if(Record_ps != (t_sAPPSDM_NvmDiagRecord *)NULL)
            {
                if((0xFFFFFFFFUL - Record_ps->activeDuration_s_u32) < elapsedSeconds_u32)
                {
                    Record_ps->activeDuration_s_u32 = 0xFFFFFFFFUL;
                }
                else
                {
                    Record_ps->activeDuration_s_u32 += elapsedSeconds_u32;
                }
            }

            /*
            * Preserve the remainder below one second for the next update.
            */
            f_Runtime_ps->activeDurationTick_u32 += elapsedSeconds_u32 * APPSDM_MILLISECONDS_PER_SECOND;
        }
    }
}

/*********************************
 * s_APPSDM_CfgSts
 *********************************/
static t_eReturnCode s_APPSDM_CfgSts(void)
{
    t_eReturnCode Ret_e = RC_OK;
    static t_uint8 s_IdxItem_u8 = 0U;

    while((s_IdxItem_u8 < APPSDM_MAX_DIAG_ITEM_MONITORING) && (Ret_e == RC_OK))
    {
        t_sAPPSDM_NvmDiagRecord NvmRecord_s;
        t_eFMKNVM_ObjectId NvmObjID_e = c_AppSdm_ObjectRegistID_ae[s_IdxItem_u8];

        Ret_e = FMKNVM_GetObject(   NvmObjID_e, 
                                    (void *)&NvmRecord_s, 
                                    (t_uint32)sizeof(NvmRecord_s));

        if(Ret_e == RC_OK)
        {
            if(s_APPSDM_IsHistoryRecordValid(&NvmRecord_s) == (t_bool)TRUE)
            {
                Ret_e = SafeMem_memcpy( (void *)&g_diagHistory_as[s_IdxItem_u8], 
                                        (const void *)&NvmRecord_s,
                                        (t_uint16)sizeof(NvmRecord_s));

                if((Ret_e == RC_OK) && (g_diagHistoryCnt_u8 < APPSDM_MAX_DIAG_ITEM_MONITORING))
                {
                    g_diagHistoryCnt_u8++;
                }
            }
            else
            {
                /*
                 * Empty/corrupted/legacy APP_SDM layout.
                 * Reinitialize the object with the new explicit marker.
                 */
                s_APPSDM_ResetHistoryRecord(&g_diagHistory_as[s_IdxItem_u8]);

                g_RqstNvmOpe_b = (t_bool)TRUE;
            }

            s_IdxItem_u8++;
        }
        else if(Ret_e == RC_WARNING_NVM_OBJECT_NOT_AVAILABLE)
        {
            s_APPSDM_ResetHistoryRecord(&g_diagHistory_as[s_IdxItem_u8]);

            s_IdxItem_u8++;
            Ret_e = RC_OK;
            g_RqstNvmOpe_b = (t_bool)TRUE;
        }
    }

    return Ret_e;
}

/*********************************
 * s_APPSDM_Operational
 *********************************/
static t_eReturnCode s_APPSDM_Operational(void)
{
    t_eReturnCode Ret_e;

    Ret_e = s_APPSDM_Ope_DiagMngmt();

    if((Ret_e == RC_OK) && (g_RqstNvmOpe_b == (t_bool)TRUE))
    {
        Ret_e = s_APPSDM_Ope_NvmMngmt();

        if(Ret_e == RC_OK)
        {
            g_RqstNvmOpe_b = (t_bool)FALSE;
        }
    }

    return Ret_e;
}

/*********************************
 * s_APPSDM_Ope_DiagMngmt
 *********************************/
static t_eReturnCode s_APPSDM_Ope_DiagMngmt(void)
{
    t_eReturnCode Ret_e = RC_OK;
    t_uint16 idxItem_u16;

    /*
     * Runtime monitoring is indexed by diagnostic ID, not by persistent
     * history slot. This fixes the previous coupling between the ten NVM
     * slots and the configured diagnostic table.
     */
    for(idxItem_u16 = 0U; (idxItem_u16 < (t_uint16)APPSDM_DIAG_ITEM_NB) && (Ret_e == RC_OK); idxItem_u16++)
    {
        t_sAPPSDM_RuntimeInfo * Runtime_ps = &g_diagRuntime_as[idxItem_u16];

        if(Runtime_ps->mngmtState_e != APPSDM_DIAG_ITEM_STATUS_OFF)
        {
            Ret_e = s_APPSDM_DiagnosticMngmt((t_eAPPSDM_DiagnosticItem)idxItem_u16, Runtime_ps, &c_AppSdm_DiagItemCfg_as[idxItem_u16]);
        }
    }

    return Ret_e;
}

/*********************************
 * s_APPSDM_Ope_NvmMngmt
 *********************************/
static t_eReturnCode s_APPSDM_Ope_NvmMngmt(void)
{
    t_eReturnCode Ret_e = RC_OK;
    t_uint8 idxItem_u8;

    /*
     * All ten logical objects belong to the diagnostic partition.
     * FMK_NVM compares bytes and only marks the partition dirty when a
     * record actually changed, so writing the ten cache objects here does
     * not imply ten physical commits.
     */
    for(idxItem_u8 = 0U; (idxItem_u8 < APPSDM_MAX_DIAG_ITEM_MONITORING) && (Ret_e == RC_OK); idxItem_u8++)
    {
        t_eFMKNVM_ObjectId NvmObjID_e = c_AppSdm_ObjectRegistID_ae[idxItem_u8];

        Ret_e = FMKNVM_SetObject(   NvmObjID_e, 
                                    (const void *)&g_diagHistory_as[idxItem_u8], 
                                    (t_uint32)sizeof(g_diagHistory_as[idxItem_u8]));

        if(Ret_e == RC_WARNING_NO_OPERATION)
        {
            Ret_e = RC_OK;
        }
        else if(Ret_e < RC_OK)
        {
            ASSERT((t_sint32)Ret_e);
        }
    }
    if(Ret_e == RC_OK)
    {
        FMKCPU_GetTick(&g_lastNvmSyncTick_u32);
    }

    return Ret_e;
}

/*********************************
 * s_APPSDM_DiagnosticMngmt
 *********************************/
static t_eReturnCode s_APPSDM_DiagnosticMngmt(   t_eAPPSDM_DiagnosticItem f_Item_e,
                                                 t_sAPPSDM_RuntimeInfo * f_Runtime_ps,
                                                 const t_sAPPSM_DiagItemCfg * f_ItemCfg_ps)
{
    t_eReturnCode Ret_e = RC_OK;
    t_uint32 currentTick_u32 = 0U;

    if((f_Item_e >= APPSDM_DIAG_ITEM_NB) || (f_Runtime_ps == (t_sAPPSDM_RuntimeInfo *)NULL) || (f_ItemCfg_ps ==
        (const t_sAPPSM_DiagItemCfg *)NULL))
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        FMKCPU_GetTick(&currentTick_u32);

        if(f_Runtime_ps->mngmtState_e == APPSDM_DIAG_ITEM_STATUS_DBC)
        {
            if(f_Runtime_ps->reportState_e == APPSDM_DIAG_ITEM_REPORT_PASS)
            {
                f_Runtime_ps->mngmtState_e = APPSDM_DIAG_ITEM_STATUS_OFF;
                f_Runtime_ps->dbcCounter_u32 = 0U;
            }
            else if((f_ItemCfg_ps->DebuncCnt_u16 == 0U) 
            || (f_Runtime_ps->dbcCounter_u32 >= (t_uint32)f_ItemCfg_ps->DebuncCnt_u16))
            {
                Ret_e = s_APPSDM_DiagStratMngmt(f_ItemCfg_ps->diagStrat_e, APPSDM_DIAG_STRAT_INHIBIT_ON);

                if(Ret_e == RC_OK)
                {
                    Ret_e = s_APPSDM_RegisterOccurrence(f_Item_e, f_Runtime_ps);
                }

                if(Ret_e == RC_OK)
                {
                    f_Runtime_ps->mngmtState_e = APPSDM_DIAG_ITEM_STATUS_ON;
                    f_Runtime_ps->activeDurationTick_u32 = currentTick_u32;
                    f_Runtime_ps->broadcastDelay_u32 = currentTick_u32;

                    FMKSRL_LOG("[DIAG] qualified item=%u count updated " "info1=%u info2=%u\r\n", (t_uint16)f_Item_e,
                        f_Runtime_ps->debugInfo1_u16, f_Runtime_ps->debugInfo2_u16);
                }
            }
            else if((currentTick_u32 - f_Runtime_ps->lastReportTick_u32) > f_ItemCfg_ps->unactiveDelay_u32)
            {
                /*
                 * The candidate disappeared before qualification.
                 * Nothing is stored in history.
                 */
                f_Runtime_ps->mngmtState_e = APPSDM_DIAG_ITEM_STATUS_OFF;
                f_Runtime_ps->reportState_e = APPSDM_DIAG_ITEM_REPORT_PASS;
                f_Runtime_ps->dbcCounter_u32 = 0U;
            }
        }
        else if(f_Runtime_ps->mngmtState_e == APPSDM_DIAG_ITEM_STATUS_ON)
        {
            s_APPSDM_UpdateActiveDuration(f_Item_e, f_Runtime_ps, currentTick_u32);

            if((f_Runtime_ps->reportState_e == APPSDM_DIAG_ITEM_REPORT_PASS) 
            || ((currentTick_u32 - f_Runtime_ps->lastReportTick_u32) > f_ItemCfg_ps->unactiveDelay_u32))
            {
                f_Runtime_ps->reportState_e = APPSDM_DIAG_ITEM_REPORT_PASS;

                Ret_e = s_APPSDM_DiagStratMngmt(f_ItemCfg_ps->diagStrat_e, APPSDM_DIAG_STRAT_INHIBIT_OFF);

                if((Ret_e == RC_OK) 
                && (g_UserCallback_pcb != (t_cbAPPSDM_DiagEventBroadcast *) NULL_FUNCTION) 
                && (f_ItemCfg_ps->notifyUser_b == (t_bool)TRUE))
                {
                    g_UserCallback_pcb(f_Item_e, APPSDM_DIAG_ITEM_REPORT_PASS, f_Runtime_ps->debugInfo1_u16, f_Runtime_ps->debugInfo2_u16);
                }

                if(Ret_e == RC_OK)
                {
                    /*
                     * Persist the final cumulative active duration.
                     * The historical entry itself remains allocated.
                     */
                    g_RqstNvmOpe_b = (t_bool)TRUE;

                    f_Runtime_ps->mngmtState_e = APPSDM_DIAG_ITEM_STATUS_OFF;
                    f_Runtime_ps->dbcCounter_u32 = 0U;
                }
            }
            else
            {
                if(((currentTick_u32 - f_Runtime_ps->broadcastDelay_u32) > (t_uint32) APPSDM_BROADCAST_TIMEOUT) 
                && (g_UserCallback_pcb != (t_cbAPPSDM_DiagEventBroadcast *) NULL_FUNCTION) 
                && (f_ItemCfg_ps->notifyUser_b == (t_bool)TRUE))
                {
                    g_UserCallback_pcb(f_Item_e, APPSDM_DIAG_ITEM_REPORT_FAIL, f_Runtime_ps->debugInfo1_u16, f_Runtime_ps->debugInfo2_u16);

                    f_Runtime_ps->broadcastDelay_u32 = currentTick_u32;
                }

                //--- NVM sync whenever the diag item status does not change ----//
                if((currentTick_u32 - g_lastNvmSyncTick_u32) > APPSDM_SYNC_NVM_ITEM)
                {
                    g_RqstNvmOpe_b = TRUE;
                }
            }
        }
    }

    return Ret_e;
}

/*********************************
 * s_APPSDM_DiagStratMngmt
 *********************************/
static t_eReturnCode s_APPSDM_DiagStratMngmt(   t_eAPPSDM_DiagnosticStrat f_diagStrat_e,
                                                t_eAPPSDM_DiagStratOpe f_stratOpe_e)
{
    t_eReturnCode Ret_e;

    if((f_diagStrat_e >= APPSDM_DIAG_STRAT_NB) && (f_diagStrat_e != APPSDM_DIAG_STRAT_NONE))
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else
    {
        Ret_e = RC_OK;

        if(f_diagStrat_e != APPSDM_DIAG_STRAT_NONE)
        {
            c_AppSdm_DiagStragies_apf[f_diagStrat_e](f_stratOpe_e);
        }
    }

    return Ret_e;
}

/*********************************
 * s_APPSDM_IsLeapYear
 *********************************/
static t_bool s_APPSDM_IsLeapYear(t_uint16 f_Year_u16)
{
    t_bool IsLeapYear_b = (t_bool)FALSE;

    if(((f_Year_u16 % 4U) == 0U) && (((f_Year_u16 % 100U) != 0U) || ((f_Year_u16 % 400U) == 0U)))
    {
        IsLeapYear_b = (t_bool)TRUE;
    }

    return IsLeapYear_b;
}

/*********************************
 * s_APPSDM_GetDaysInMonth
 *********************************/
static t_uint8 s_APPSDM_GetDaysInMonth(   t_uint16 f_Year_u16,
                                          t_uint8 f_Month_u8)
{
    static const t_uint8 DaysPerMonth_au8[12U] =
    {
        31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U
    };

    t_uint8 Days_u8 = 0U;

    if((f_Month_u8 >= 1U) && (f_Month_u8 <= 12U))
    {
        Days_u8 = DaysPerMonth_au8[f_Month_u8 - 1U];

        if((f_Month_u8 == 2U) && (s_APPSDM_IsLeapYear(f_Year_u16) == (t_bool)TRUE))
        {
            Days_u8 = 29U;
        }
    }

    return Days_u8;
}

/*********************************
 * s_APPSDM_DateTimeToTimestamp
 *********************************/
static t_eReturnCode s_APPSDM_DateTimeToTimestamp(   const t_sFMKCPU_DateTime * f_DateTime_ps,
                                                     t_uint32 * f_Timestamp_pu32)
{
    t_eReturnCode Ret_e = RC_OK;
    t_uint32 days_u32 = 0U;
    t_uint16 year_u16;
    t_uint8 month_u8;

    if((f_DateTime_ps == (const t_sFMKCPU_DateTime *)NULL) || (f_Timestamp_pu32 == (t_uint32 *)NULL))
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else if((f_DateTime_ps->date_s.year_u16 < 2000U) || (f_DateTime_ps->date_s.year_u16 > 2099U) || (f_DateTime_ps->date_s.month_u8 < 1U) ||
            (f_DateTime_ps->date_s.month_u8 > 12U) || (f_DateTime_ps->date_s.day_u8 < 1U) || (f_DateTime_ps->date_s.day_u8 >
             s_APPSDM_GetDaysInMonth(f_DateTime_ps->date_s.year_u16, f_DateTime_ps->date_s.month_u8)) ||
            (f_DateTime_ps->time_s.hour_u8 > 23U) || (f_DateTime_ps->time_s.minute_u8 > 59U) || (f_DateTime_ps->time_s.second_u8 > 59U))
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    else
    {
        for(year_u16 = 2000U; year_u16 < f_DateTime_ps->date_s.year_u16; year_u16++)
        {
            days_u32 += (s_APPSDM_IsLeapYear(year_u16) == (t_bool)TRUE) ? 366U : 365U;
        }

        for(month_u8 = 1U; month_u8 < f_DateTime_ps->date_s.month_u8; month_u8++)
        {
            days_u32 += s_APPSDM_GetDaysInMonth(f_DateTime_ps->date_s.year_u16, month_u8);
        }

        days_u32 += (t_uint32)
            f_DateTime_ps->date_s.day_u8 - 1U;

        *f_Timestamp_pu32 =
            (days_u32 * APPSDM_SECONDS_PER_DAY) + ((t_uint32) f_DateTime_ps->time_s.hour_u8 * APPSDM_SECONDS_PER_HOUR) + ((t_uint32)
             f_DateTime_ps->time_s.minute_u8 * APPSDM_SECONDS_PER_MINUTE) + (t_uint32)
            f_DateTime_ps->time_s.second_u8;
    }

    return Ret_e;
}

/*********************************
 * s_APPSDM_GetCurrentTimestamp
 *********************************/
static t_eReturnCode s_APPSDM_GetCurrentTimestamp(t_uint32 * f_Timestamp_pu32)
{
    t_eReturnCode Ret_e = RC_OK;
    t_bool isDateTimeValid_b = (t_bool)FALSE;
    t_sFMKCPU_DateTime DateTime_s;

    if(f_Timestamp_pu32 == (t_uint32 *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    else
    {
        *f_Timestamp_pu32 = 0U;

        Ret_e = FMKCPU_IsDateTimeValid(&isDateTimeValid_b);

        /*
         * RTC availability must never block diagnostic qualification.
         * A zero timestamp explicitly means "date unavailable".
         */
        if((Ret_e != RC_OK) || (isDateTimeValid_b == (t_bool)FALSE))
        {
            *f_Timestamp_pu32 = 0U;
            Ret_e = RC_OK;
        }
        else
        {
            Ret_e = FMKCPU_GetDateTime(&DateTime_s);

            if(Ret_e == RC_OK)
            {
                Ret_e = s_APPSDM_DateTimeToTimestamp(&DateTime_s, f_Timestamp_pu32);
            }

            if(Ret_e != RC_OK)
            {
                *f_Timestamp_pu32 = 0U;
                Ret_e = RC_OK;
            }
        }
    }

    return Ret_e;
}

/*********************************
 * s_APPSDM_TimestampToDateTime
 *********************************/
static void s_APPSDM_TimestampToDateTime(   t_uint32 f_Timestamp_u32,
                                            t_sAPPSDM_DateTime * f_DateTime_ps)
{
    t_uint32 days_u32;
    t_uint32 secondsInDay_u32;
    t_uint16 year_u16 = 2000U;
    t_uint8 month_u8 = 1U;

    if(f_DateTime_ps == (t_sAPPSDM_DateTime *)NULL)
    {
        return;
    }

    days_u32 = f_Timestamp_u32 / APPSDM_SECONDS_PER_DAY;

    secondsInDay_u32 = f_Timestamp_u32 % APPSDM_SECONDS_PER_DAY;

    while(year_u16 < 2099U)
    {
        t_uint32 daysInYear_u32 = (s_APPSDM_IsLeapYear(year_u16) == (t_bool)TRUE) ? 366U : 365U;

        if(days_u32 < daysInYear_u32)
        {
            break;
        }

        days_u32 -= daysInYear_u32;
        year_u16++;
    }

    while(month_u8 <= 12U)
    {
        t_uint8 daysInMonth_u8 = s_APPSDM_GetDaysInMonth(year_u16, month_u8);

        if(days_u32 < (t_uint32)daysInMonth_u8)
        {
            break;
        }

        days_u32 -= (t_uint32)daysInMonth_u8;
        month_u8++;
    }

    f_DateTime_ps->year_u16 = year_u16;
    f_DateTime_ps->month_u8 = month_u8;
    f_DateTime_ps->day_u8 = (t_uint8)(days_u32 + 1U);

    f_DateTime_ps->hour_u8 = (t_uint8)
        (secondsInDay_u32 / APPSDM_SECONDS_PER_HOUR);

    secondsInDay_u32 %= APPSDM_SECONDS_PER_HOUR;

    f_DateTime_ps->minute_u8 = (t_uint8)
        (secondsInDay_u32 / APPSDM_SECONDS_PER_MINUTE);

    f_DateTime_ps->second_u8 = (t_uint8)
        (secondsInDay_u32 % APPSDM_SECONDS_PER_MINUTE);
}

//************************************************************************************
// End of File
//************************************************************************************