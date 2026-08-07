/*********************************************************************
 * @file        APP_SDM.h
 * @brief       Application diagnostic manager public interface.
 *********************************************************************/

#ifndef APPSDM_H_INCLUDED
#define APPSDM_H_INCLUDED

// ********************************************************************
// *                      Includes
// ********************************************************************
#include "APP_CFG/ConfigFiles/APPSDM_ConfigPublic.h"

// ********************************************************************
// *                      Types
// ********************************************************************
///@brief report diag event status
typedef enum
{
    APPSDM_DIAG_ITEM_REPORT_PASS = 0x00,
    APPSDM_DIAG_ITEM_REPORT_FAIL,

    APPSDM_DIAG_ITEM_STATE_NB
} t_eAPPSDM_DiagnosticReport;

/**
 * @brief Calendar representation used by the diagnostic history API.
 */
typedef struct
{
    t_uint16 year_u16;
    t_uint8 month_u8;
    t_uint8 day_u8;
    t_uint8 hour_u8;
    t_uint8 minute_u8;
    t_uint8 second_u8;
} t_sAPPSDM_DateTime;

/**
 * @brief Public snapshot of one persistent diagnostic history slot.
 *
 * @note activeDuration_s_u32 is cumulative over all qualified occurrences.
 *       Debug values correspond to the latest qualified occurrence.
 */
typedef struct
{
    t_eAPPSDM_DiagnosticItem itemId_e;
    t_sAPPSDM_DateTime firstOccurrence_s;
    t_uint32 occurrenceCount_u32;
    t_uint32 activeDuration_s_u32;
    t_uint16 debugInfo1_u16;
    t_uint16 debugInfo2_u16;
    t_bool isActive_b;
    t_bool isFirstOccurrenceDateValid_b;
} t_sAPPSDM_DiagHistoryItem;

typedef void (t_cbAPPSDM_DiagEventBroadcast)(   t_eAPPSDM_DiagnosticItem f_item_e,
                                                t_eAPPSDM_DiagnosticReport f_reportState_e,
                                                t_uint16 f_debugInfo1_u16,
                                                t_uint16 f_debugInfo2_u16);

// ********************************************************************
// *                      Public functions - Prototypes
// ********************************************************************
        /**
    *
    *	@brief      Perform all init action to this module
    *
    *
    * @retval RC_OK                             @ref RC_OK
    *
    */
    t_eReturnCode APPSDM_Init(void);
    /**
    *
    *	@brief      Perform all cyclic action to this module   
    *   @note     
    * 
    * 
    * @retval RC_OK                               @ref RC_OK
    * @retval RC_WARNING_WRONG_STATE              @ref RC_ERROR_WARNING_STATE
    * @retval RC_WARNING_BUSY                     @ref RC_WARNING_BUSY
    */
    t_eReturnCode APPSDM_Cyclic(void);
    /**
    *
    *	@brief Function to know the module state 
    *	@param[in]  f_State_pe : store the value, value from @ref t_eCyclicModState
    *
    *   @retval RC_OK                             @ref RC_OK
    *   @retval RC_ERROR_PTR_NULL                 @ref RC_ERROR_PTR_NUL
    */
    t_eReturnCode APPSDM_GetState(t_eCyclicModState *f_State_pe);
    /**
    *
    *	@brief Function to update the module state 
    *
    *	@param[in]  f_State_e : the new value, value from @ref t_eCyclicModState
    *
    *   @retval RC_OK                             @ref RC_OK
    *   @retval RC_ERROR_PTR_NULL                 @ref RC_ERROR_PTR_NUL
    */
    t_eReturnCode APPSDM_SetState(t_eCyclicModState f_State_e);

    /**
     * @brief Report the current result of one diagnostic item.
     *
     * @note Repeated FAIL reports belonging to the same active occurrence do
     *       not increment the persistent occurrence counter. The counter is
     *       incremented only when the diagnostic becomes qualified ON after
     *       debounce.
     */
    void APPSDM_ReportDiagEvnt( t_eAPPSDM_DiagnosticItem f_item_e,
                                t_eAPPSDM_DiagnosticReport f_reportState_e,
                                t_uint16 f_debugInfo1_u16,
                                t_uint16 f_debugInfo2_u16);

    /**
     * @brief Return the current runtime state of one diagnostic item.
     */
    t_eReturnCode APPSDM_GetDiagStatus( t_eAPPSDM_DiagnosticItem f_item_e,
                                        t_eAPPSDM_DiagnosticReport * f_state_pe);

    /**
     * @brief Return the number of currently stored diagnostic history entries.
     *
     * @param[out] f_Count_pu8 Number of valid entries from 0 to
     *                         APPSDM_MAX_DIAG_ITEM_MONITORING.
     */
    t_eReturnCode APPSDM_GetDiagHistoryCount(t_uint8 * f_Count_pu8);

    /**
     * @brief Read one persistent diagnostic history slot.
     *
     * @param[in] f_SlotIdx_u8 Slot index in
     *                         [0, APPSDM_MAX_DIAG_ITEM_MONITORING).
     * @param[out] f_ItemInfo_ps Public diagnostic snapshot.
     * @param[out] f_IsAvailable_pb TRUE when the selected slot contains a
     *                              diagnostic entry, FALSE when it is empty.
     *
     * @note An inactive diagnostic remains available until history is explicitly
     *       cleared or the oldest-entry replacement policy reuses its slot.
     */
    t_eReturnCode APPSDM_GetDiagHistoryItem(t_uint8 f_SlotIdx_u8,
                                            t_sAPPSDM_DiagHistoryItem * f_ItemInfo_ps,
                                            t_bool * f_IsAvailable_pb);

    /**
     * @brief Clear the persistent diagnostic history.
     *
     * @note Current active diagnostics are not stopped. They are immediately
     *       recreated as fresh history entries with occurrenceCount = 1 and
     *       activeDuration = 0.
     */
    t_eReturnCode APPSDM_ClearDiagHistory(void);

    t_eReturnCode APPSDM_AddCallbackEvnt(t_cbAPPSDM_DiagEventBroadcast * f_evntCallback_pcb);

#endif // APPSDM_H_INCLUDED

//************************************************************************************
// End of File
//************************************************************************************
