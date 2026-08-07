/*********************************************************************
 * @file        item.h
 * @brief       Template_BriefDescription.
 * @details     TemplateDetailsDescription.\n
 *
 * @author      xxxxxx
 * @date        jj/mm/yyyy
 * @version     1.0
 */






// ********************************************************************
// *                      Includes
// ********************************************************************
#include "./APP_SDM.h"
#include "APP_CFG/ConfigFiles/APPSDM_ConfigPrivate.h"
#include "APP_CTRL/APP_SYS/Src/APP_SYS.h"

#include "FMK_HAL/FMK_CPU/Src/FMK_CPU.h"
#include "Library/SafeMem/SafeMem.h"
// ********************************************************************
// *                      Defines
// ********************************************************************

// ********************************************************************
// *                      Types
// ********************************************************************
/* CAUTION : Automatic generated code section for Enum: Start */

/* CAUTION : Automatic generated code section for Enum: End */
//-----------------------------ENUM TYPES-----------------------------//
typedef enum 
{
    APPSDM_DIAG_ITEM_STATUS_OFF = 0x00,
    APPSDM_DIAG_ITEM_STATUS_DBC,
    APPSDM_DIAG_ITEM_STATUS_ON,

    APPSDM_DIAG_ITEM_STATUS_NB,
} t_eAPPSDM_ItemState;

/* CAUTION : Automatic generated code section for Structure: Start */

/* CAUTION : Automatic generated code section for Structure: End */
//-----------------------------STRUCT TYPES---------------------------//
///@brief Runtime Info for diagnostic item
typedef struct 
{
    t_eAPPSDM_ItemState         mngmtState_e;
    t_uint32                    dbcCounter_u32;
    t_uint32                    broadcastDelay_u32;
} t_sAPPSDM_RuntimeInfo;

/// @brief Diagnostic item descripter 
typedef struct 
{
    t_eAPPSDM_DiagnosticItem    itemId_e;
    t_eAPPSDM_DiagnosticReport  reportstate_e;
    t_uint32                    reportTime_u32;
    t_uint32                    activeDuration_u32;
    t_uint16                    debugInfo1_u16;
    t_uint16                    debugInfo2_u16;                 
} t_sAPPSDM_DescInfo;

///@brief Diagnostic item info
typedef struct 
{
    t_sAPPSDM_RuntimeInfo runtime_s;
    t_sAPPSDM_DescInfo desc_s;
} t_sAPPSDM_DiagItemInfo;
/* CAUTION : Automatic generated code section : Start */

/* CAUTION : Automatic generated code section : End */
//-----------------------------TYPEDEF TYPES---------------------------//
// ********************************************************************
// *                      Prototypes
// ********************************************************************
	
// ********************************************************************
// *                      Variables
// ********************************************************************

///@brief
static t_eCyclicModState g_AppSdm_ModState_e = STATE_CYCLIC_CFG;

///@brief
static t_sAPPSDM_DiagItemInfo g_diagItemInfo_as[APPSDM_MAX_DIAG_ITEM_MONITORING];

/**< */
static t_bool g_rqstDiagMngmt_b = (t_bool)False;

///@brief 
static t_bool g_RqstNvmOpe_b = (t_bool)FALSE;

///@brief
static t_uint8 g_diagItemCnt_u8 = (t_uint8)0;

///@brief
static t_cbAPPSDM_DiagEventBroadcast * g_UserCallback_pcb = (t_cbAPPSDM_DiagEventBroadcast *)NULL_FUNCTION;

///@brief
static t_uint8 g_freeItemIdx_u8 = (t_uint8)0;

///@brief
static t_uint8 g_MaxIdxRegistration_u8 = (t_uint8)0;
//********************************************************************************
//                      Local functions - Prototypes
//********************************************************************************
/**
 *
 *	@brief  Update g_freeItemIdx_u8 and g_MaxIdxRegistration_u8.\n
 */
static void s_APPSDM_FoundFreeIdx(void);
/**
 *
 *	@brief      
 *  @note       
 *              
 * @retval RC_OK                               @ref RC_OK
 * @retval RC_WARNING_WRONG_STATE              @ref RC_ERROR_WARNING_STATE
 * @retval RC_WARNING_BUSY                     @ref RC_WARNING_BUSY
 *
 */
static t_eReturnCode s_APPSDM_Ope_DiagMngmt(void);
/**
 *
 *	@brief      
 *  @note       
 *              
 * @retval RC_OK                               @ref RC_OK
 * @retval RC_WARNING_WRONG_STATE              @ref RC_ERROR_WARNING_STATE
 * @retval RC_WARNING_BUSY                     @ref RC_WARNING_BUSY
 *
 */
static t_eReturnCode s_APPSDM_Ope_NvmMngmt(void);
/**
 *
 *	@brief      
 *  @note       
 *              
 * @retval RC_OK                               @ref RC_OK
 * @retval RC_WARNING_WRONG_STATE              @ref RC_ERROR_WARNING_STATE
 * @retval RC_WARNING_BUSY                     @ref RC_WARNING_BUSY
 *
 */
static t_eReturnCode s_APPSDM_DiagnosticMngmt(  t_sAPPSDM_DiagItemInfo * f_itemInfo_ps,
                                                t_sAPPSM_DiagItemCfg * f_itemCfg_ps );
/**
 *
 *	@brief      s_APPSDM_DiagStratMngmt
 *  @note       
 *              
 * @retval RC_OK                               @ref RC_OK
 * @retval RC_WARNING_WRONG_STATE              @ref RC_ERROR_WARNING_STATE
 * @retval RC_WARNING_BUSY                     @ref RC_WARNING_BUSY
 *
 */
static t_eReturnCode s_APPSDM_DiagStratMngmt(t_eAPPSDM_DiagnosticStrat f_diagStrat_e,
                                             t_eAPPSDM_DiagStratOpe f_stratOpe_e);
/**
 *
 *	@brief      Perform cfg operation for this module.\n
 *  @note       Reach the Diagnostic Status before the system 
 *              was turn down from NVM
 *              
 * @retval RC_OK                               @ref RC_OK
 * @retval RC_WARNING_WRONG_STATE              @ref RC_ERROR_WARNING_STATE
 * @retval RC_WARNING_BUSY                     @ref RC_WARNING_BUSY
 *
 */
static t_eReturnCode s_APPSDM_CfgSts(void);
/**
 *
 *	@brief      Perform cyclic operation for this module.\n
 *  @note       
 *              
 * @retval RC_OK                               @ref RC_OK
 * @retval RC_WARNING_WRONG_STATE              @ref RC_ERROR_WARNING_STATE
 * @retval RC_WARNING_BUSY                     @ref RC_WARNING_BUSY
 *
 */
static t_eReturnCode s_APPSDM_Operational(void);
//****************************************************************************
//                      Public functions - Implementation
//********************************************************************************
/*********************************
 * APPSDM_Init
 *********************************/
t_eReturnCode APPSDM_Init(void)
{
    t_uint8 idxItem_u8; 

    for(idxItem_u8 = (t_uint8)0 ; idxItem_u8 < APPSDM_MAX_DIAG_ITEM_MONITORING ; idxItem_u8++)
    {
        g_diagItemInfo_as[idxItem_u8].runtime_s.broadcastDelay_u32 = (t_uint16)0;
        g_diagItemInfo_as[idxItem_u8].runtime_s.mngmtState_e = APPSDM_DIAG_ITEM_STATUS_OFF;
        g_diagItemInfo_as[idxItem_u8].runtime_s.dbcCounter_u32 = 0U;
        g_diagItemInfo_as[idxItem_u8].desc_s.itemId_e = APPSDM_DIAG_ITEM_NB;
        g_diagItemInfo_as[idxItem_u8].desc_s.reportstate_e = APPSDM_DIAG_ITEM_REPORT_PASS;
        g_diagItemInfo_as[idxItem_u8].desc_s.debugInfo1_u16 = (t_uint16)0;
        g_diagItemInfo_as[idxItem_u8].desc_s.debugInfo2_u16 = (t_uint16)0;
        g_diagItemInfo_as[idxItem_u8].desc_s.reportTime_u32 = (t_uint32)0;
        g_diagItemInfo_as[idxItem_u8].desc_s.activeDuration_u32 = 0U;
    }
    g_freeItemIdx_u8 = (t_uint8)0;
    g_MaxIdxRegistration_u8 = (t_uint8)0;
    g_diagItemCnt_u8 = (t_uint8)0;
    g_rqstDiagMngmt_b = (t_bool)False;

    return RC_OK;
}
/*********************************
 * APPSDM_Init
 *********************************/
t_eReturnCode APPSDM_Cyclic(void)
{
    t_eReturnCode Ret_e = RC_OK;

    switch (g_AppSdm_ModState_e)
    {
        case STATE_CYCLIC_CFG:
        {
            Ret_e = s_APPSDM_CfgSts();
            if(Ret_e == RC_OK)
            {
                //--- set flag to read diagnostic from nvm ---//
                g_rqstDiagMngmt_b = TRUE;
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
        case STATE_CYCLIC_BUSY:
        default:
            Ret_e = RC_OK;
            break;
    }
    return Ret_e;
}

/*********************************
 * APPSDM_GetState
 *********************************/
t_eReturnCode APPSDM_GetState(t_eCyclicModState *f_State_pe)
{
    t_eReturnCode Ret_e = RC_OK;
    
    if(f_State_pe == (t_eCyclicModState *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    if(Ret_e == RC_OK)
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
void APPSDM_ReportDiagEvnt( t_eAPPSDM_DiagnosticItem f_item_e,
                            t_eAPPSDM_DiagnosticReport f_reportState_e,
                            t_uint16 f_debugInfo1_u16,
                            t_uint16 f_debugInfo2_u16)
{
    t_sAPPSDM_DiagItemInfo * itemInfo_ps;
    t_uint8 idxItem_u8 = (t_uint8)0;
    t_bool initItem_b = (t_bool)True;
    t_uint32 currentTime_u32;

    if((f_item_e >= APPSDM_DIAG_ITEM_NB)
    || (f_reportState_e >= APPSDM_DIAG_ITEM_STATE_NB))
    {
        ASSERT((t_sint32)0);
    }
    else if(APPSDM_DIAG_MNGMT_STATUS == (t_bool)TRUE)
    {
        FMKCPU_GetTick(&currentTime_u32);
        //----- See if at least one item is active -----//
        if(g_rqstDiagMngmt_b == (t_bool)False)
        {
            //----- If diag PASS, no operation to do -----//
            if(f_reportState_e == APPSDM_DIAG_ITEM_REPORT_PASS)
            {
                initItem_b = (t_bool)False;
            }
        }
        else 
        {
            //----- At least one of the diag is ON and we have to fount at 
            //      if it it already ON or it's a new one -----//
            for(idxItem_u8 = (t_uint8)0 ; idxItem_u8 < g_MaxIdxRegistration_u8 ; idxItem_u8++)
            {
                itemInfo_ps = (&g_diagItemInfo_as[idxItem_u8]);

                if(f_item_e == itemInfo_ps->desc_s.itemId_e)
                {
                    initItem_b = (t_bool)False;
                    itemInfo_ps->runtime_s.dbcCounter_u32++;
                    itemInfo_ps->desc_s.reportstate_e = f_reportState_e;
                    itemInfo_ps->desc_s.debugInfo1_u16 = f_debugInfo1_u16;
                    itemInfo_ps->desc_s.debugInfo2_u16 = f_debugInfo2_u16;
                    g_RqstNvmOpe_b = TRUE;
                    break;
                }
            }
            //---- if not found initItem_b still TRUE & reportState = PASS
            //      means the diag item is no longer under control -> NO_OPE----//
            if((initItem_b == (t_bool)True)
            && (f_reportState_e == APPSDM_DIAG_ITEM_REPORT_PASS))
            {
                initItem_b = (t_bool)False;
            }
        }
        //----- See if we have to register a new diag item -----//
        if(initItem_b == (t_bool)True)
        {
            if(g_rqstDiagMngmt_b == (t_bool)False)
            {
                g_rqstDiagMngmt_b = (t_bool)True;
            }
            if(g_diagItemCnt_u8 >= APPSDM_MAX_DIAG_ITEM_MONITORING)
            {
                ASSERT((t_sint32)g_diagItemCnt_u8);
            }
            else 
            {
                //----- Copy data -----//
                itemInfo_ps = (t_sAPPSDM_DiagItemInfo *)(&g_diagItemInfo_as[g_freeItemIdx_u8]);
                
                itemInfo_ps->desc_s.debugInfo1_u16 = f_debugInfo1_u16;
                itemInfo_ps->desc_s.debugInfo2_u16 = f_debugInfo2_u16;
                itemInfo_ps->desc_s.itemId_e = f_item_e;
                itemInfo_ps->desc_s.reportstate_e = f_reportState_e;
                itemInfo_ps->desc_s.reportTime_u32 = currentTime_u32;
                itemInfo_ps->runtime_s.broadcastDelay_u32 = currentTime_u32;
                itemInfo_ps->runtime_s.dbcCounter_u32 = (t_uint32)1;

                //----- log ----//
                FMKSRL_LOG("[%d] : New Diagnostic Item -> %d, info1 %d: , Info2 : %d\r\n",
                            itemInfo_ps->desc_s.reportTime_u32,
                            (t_uint16)f_item_e,
                            f_debugInfo1_u16,
                            f_debugInfo2_u16);
                //---- check if actions has to be set now or later -----//
                if(c_AppSdm_DiagItemCfg_as[f_item_e].DebuncCnt_u16 == (t_uint16)0)
                {
                    (void)s_APPSDM_DiagStratMngmt(c_AppSdm_DiagItemCfg_as[f_item_e].diagStrat_e,
                                                    APPSDM_DIAG_STRAT_INHIBIT_ON);

                    itemInfo_ps->runtime_s.mngmtState_e = APPSDM_DIAG_ITEM_STATUS_ON;    
                }
                else 
                {
                    itemInfo_ps->runtime_s.mngmtState_e = APPSDM_DIAG_ITEM_STATUS_DBC;
                }

                //----- Update General Information -----//
                s_APPSDM_FoundFreeIdx();
                g_diagItemCnt_u8 += (t_uint8)1;
            }
        }
    }

    return;
}

/*********************************
 * APPSDM_GetDiagStatus
 *********************************/
t_eReturnCode APPSDM_GetDiagStatus( t_eAPPSDM_DiagnosticItem f_item_e,
                                    t_eAPPSDM_DiagnosticReport * f_reportState_pe)
{
    t_eReturnCode Ret_e = RC_OK;
    t_bool itemFound_b = (t_bool)False;
    t_uint8 idxItem_u8;

    if(f_item_e >= APPSDM_DIAG_ITEM_NB)
    {
        Ret_e = RC_ERROR_PARAM_INVALID;
    }
    if(f_reportState_pe == (t_eAPPSDM_DiagnosticReport *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    if(Ret_e == RC_OK)
    {
        //----- If Counter Diag is 0, the diag is PASS -----//
        if(g_diagItemCnt_u8 == (t_uint8)0)
        {
            *f_reportState_pe = APPSDM_DIAG_ITEM_REPORT_PASS;
        }
        else
        {   
            //----- Loop to know if the item is repertory -----//
            for(idxItem_u8  = (t_uint8)0 ; idxItem_u8 < g_MaxIdxRegistration_u8 ; idxItem_u8++)
            {
                if(f_item_e == g_diagItemInfo_as[idxItem_u8].desc_s.itemId_e)
                {
                    itemFound_b = (t_bool)True;
                    *f_reportState_pe = g_diagItemInfo_as[idxItem_u8].desc_s.reportstate_e;
                    break;
                }
            }

            //----- Item not found -----//
            if(itemFound_b == (t_bool)False)
            {
                *f_reportState_pe = APPSDM_DIAG_ITEM_REPORT_PASS;
            }
        }
    }

    return Ret_e;
}

t_eReturnCode APPSDM_ResetDiagEvnt(void)
{
    t_eReturnCode Ret_e = RC_OK; 
    t_uint8 idxItem_u8 = (t_uint8)0;

    //----- if diagCounter == 0, already Reset -----//
    if(g_diagItemCnt_u8 != (t_uint8)0)
    {
        //----- Loop to know if the item is repertory -----//
        for(idxItem_u8  = (t_uint8)0 ; idxItem_u8 < g_MaxIdxRegistration_u8 ; idxItem_u8++)
        {
           g_diagItemInfo_as[idxItem_u8].runtime_s.mngmtState_e = APPSDM_DIAG_ITEM_STATUS_OFF; 
        }

        //----- Update max registration -----//
        g_MaxIdxRegistration_u8 = (t_uint8)0;
        g_freeItemIdx_u8 = (t_uint8)0;
        g_diagItemCnt_u8 = (t_uint8)0;
        g_rqstDiagMngmt_b = (t_bool)False;
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
    if(g_UserCallback_pcb != (t_cbAPPSDM_DiagEventBroadcast *)NULL_FUNCTION)
    {
        Ret_e = RC_ERROR_ALREADY_CONFIGURED;
    }
    if(Ret_e == RC_OK)
    {
        g_UserCallback_pcb = f_evntCallback_pcb;
    }

    return Ret_e;
}
//********************************************************************************
//                      Local functions - Implementation
//********************************************************************************
/*********************************
 * s_APPSDM_FoundFreeIdx
 *********************************/
static void s_APPSDM_FoundFreeIdx(void)
{
    t_uint8 idxItem_u8 = (t_uint8)0;

    //----- if we reached the maximum idx, just replace the last one 
    //      it's not a problem 'cause, the CPU will be rebooted or nothing move

    if(g_MaxIdxRegistration_u8 < APPSDM_MAX_DIAG_ITEM_MONITORING)
    {
        g_freeItemIdx_u8 = (t_uint8)(g_MaxIdxRegistration_u8 + 1);
        g_MaxIdxRegistration_u8 += (t_uint8)1;
    }
    else 
    {
        //----- there is a good chance that the first or second element 
        //      if the array is now free -----//
        for(idxItem_u8 = (t_uint8)0 ; idxItem_u8 < APPSDM_MAX_DIAG_ITEM_MONITORING ; idxItem_u8++)
        {
            if(g_diagItemInfo_as[idxItem_u8].runtime_s.mngmtState_e == APPSDM_DIAG_ITEM_STATUS_OFF)
            {
                g_freeItemIdx_u8 = idxItem_u8;
                break;
            }
        }
    }
    return;
}

/*********************************
 * s_APPSDM_CfgSts
 *********************************/
static t_eReturnCode s_APPSDM_CfgSts(void)
{
    t_eReturnCode Ret_e = RC_OK;
    static t_uint8 s_IdxItem_u8 = 0;

    while((s_IdxItem_u8 < APPSDM_MAX_DIAG_ITEM_MONITORING) && (Ret_e == RC_OK))
    {
        t_sAPPSDM_DiagItemInfo * ItemInfo_ps = &g_diagItemInfo_as[s_IdxItem_u8];
        t_eFMKNVM_ObjectId NvmObjID_e = c_AppSdm_ObjectRegistID_ae[s_IdxItem_u8];
        t_sAPPSDM_DescInfo descItemInfo_s;
        Ret_e = FMKNVM_GetObject(   NvmObjID_e,
                                    (void *)&descItemInfo_s,
                                    sizeof(descItemInfo_s));
        if(Ret_e == RC_OK)
        {
            
            Ret_e = SafeMem_memcpy( (void *)&ItemInfo_ps->desc_s,
                                    (const void *)&descItemInfo_s,
                                    sizeof(descItemInfo_s));

            if(Ret_e == RC_OK)
            {
                s_IdxItem_u8++;

                //--- update runtime mngmt status ---//
                if(descItemInfo_s.reportstate_e == APPSDM_DIAG_ITEM_REPORT_FAIL)
                {
                    ItemInfo_ps->runtime_s.mngmtState_e = APPSDM_DIAG_ITEM_STATUS_DBC;
                    ItemInfo_ps->runtime_s.dbcCounter_u32 = 1;
                }
            }
        }
        else if(Ret_e == RC_WARNING_NVM_OBJECT_NOT_AVAILABLE)
        {
            //---- EEPROM corrupted ----//
            s_IdxItem_u8++;
            Ret_e = RC_OK;

            //--- set flag to push default value anyway --//
            if(g_RqstNvmOpe_b == FALSE)
            {
                g_RqstNvmOpe_b = TRUE;
            }
        }
    }

    return Ret_e;
}

/*********************************
 * s_APPSDM_Operational
 *********************************/
static t_eReturnCode s_APPSDM_Operational(void)
{
    t_eReturnCode Ret_e = RC_OK;
    //----- Diagnostic Managment Requested -----//
    if(g_rqstDiagMngmt_b == (t_bool)True)
    {
        Ret_e = s_APPSDM_Ope_DiagMngmt();
    }

    if((Ret_e == RC_OK)
    && (g_RqstNvmOpe_b == TRUE))
    {
        Ret_e = s_APPSDM_Ope_NvmMngmt();
        if(Ret_e == RC_OK)
        {
            g_RqstNvmOpe_b = FALSE;
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
    t_uint8 idxItem_u8;

    for(idxItem_u8 = (t_uint8)0 ; idxItem_u8 < g_MaxIdxRegistration_u8 ; idxItem_u8++)
    {
        if(g_diagItemInfo_as[idxItem_u8].runtime_s.mngmtState_e != APPSDM_DIAG_ITEM_STATUS_OFF)
        {
            Ret_e = s_APPSDM_DiagnosticMngmt(   &g_diagItemInfo_as[idxItem_u8], 
                                                &c_AppSdm_DiagItemCfg_as[idxItem_u8]);
        }
    }

    //----- Update g_MaxIdxRegistration_u8 -----//
    if((g_MaxIdxRegistration_u8 > (t_uint8)0)
    && (g_diagItemInfo_as[(g_MaxIdxRegistration_u8 -(t_uint8)1)].runtime_s.mngmtState_e == APPSDM_DIAG_ITEM_STATUS_OFF))
    {
        if(g_MaxIdxRegistration_u8 > (t_uint8)1)
        {
            g_MaxIdxRegistration_u8 -= (t_uint8)1;
        }
        else 
        {
            g_MaxIdxRegistration_u8 = (t_uint8)0;
        }
        g_freeItemIdx_u8 = g_MaxIdxRegistration_u8;
    }
    if(g_diagItemCnt_u8 == (t_uint8)0)
    {
        g_rqstDiagMngmt_b = (t_bool)False;
    }

    return Ret_e;
}

/*********************************
 * s_APPSDM_Ope_NvmMngmt
 *********************************/
static t_eReturnCode s_APPSDM_Ope_NvmMngmt(void)
{
    t_eReturnCode Ret_e;

    Ret_e = RC_OK;

    for(t_uint8 idxItem_u8 = 0; 
        (idxItem_u8 < APPSDM_MAX_DIAG_ITEM_MONITORING) && (Ret_e == RC_OK) ; 
    idxItem_u8++)
    {
        t_sAPPSDM_DiagItemInfo * ItemInfo_ps = &g_diagItemInfo_as[idxItem_u8];
        t_eFMKNVM_ObjectId NvmObjID_e = c_AppSdm_ObjectRegistID_ae[idxItem_u8];

        Ret_e = FMKNVM_SetObject(   NvmObjID_e,
                                    (const void *)&ItemInfo_ps->desc_s,
                                    (t_uint32)sizeof(ItemInfo_ps->desc_s));
        if(Ret_e == RC_WARNING_NO_OPERATION)
        {
            Ret_e = RC_OK;
        }
        else if(Ret_e < RC_OK)
        {
            ASSERT((t_sint32)Ret_e);
        }
    }

    return Ret_e;
}

/*********************************
 * s_APPSDM_DiagnosticMngmt
 *********************************/
static t_eReturnCode s_APPSDM_DiagnosticMngmt(  t_sAPPSDM_DiagItemInfo * f_itemInfo_ps,
                                                t_sAPPSM_DiagItemCfg * f_itemCfg_ps )
{
    t_eReturnCode Ret_e = RC_OK;
    t_uint32 currentTime_u32 = (t_uint32)0;

    if(f_itemCfg_ps == (t_sAPPSM_DiagItemCfg *)NULL
    || f_itemInfo_ps == (t_sAPPSDM_DiagItemInfo *)NULL)
    {
        Ret_e = RC_ERROR_PTR_NULL;
    }
    if(Ret_e == RC_OK)
    {
        FMKCPU_GetTick(&currentTime_u32);
        //----- Diagnostic is still Repertory as ON -----//
        if(f_itemInfo_ps->desc_s.reportstate_e == APPSDM_DIAG_ITEM_REPORT_FAIL)
        {
            //----- Diagnostic is in Debuncer State -----//
            if(f_itemInfo_ps->runtime_s.mngmtState_e == APPSDM_DIAG_ITEM_STATUS_DBC)
            {
                //---- if we haven't get another report within DebuncCnt_u16
                //      consider the error inactive ----//
                if(f_itemInfo_ps->runtime_s.dbcCounter_u32 < (t_uint32)f_itemCfg_ps->DebuncCnt_u16)
                {
                    if((currentTime_u32 - f_itemInfo_ps->desc_s.reportTime_u32) > f_itemCfg_ps->unactiveDelay_u32)
                    {
                        f_itemInfo_ps->runtime_s.mngmtState_e = APPSDM_DIAG_ITEM_STATUS_OFF;
                        //----- Update Information -----//
                        if(g_diagItemCnt_u8 > (t_uint8)0)
                        {
                            g_diagItemCnt_u8 -= (t_uint8)1;
                        }
                    }
                }
                //---- diag item is ON -----//
                else 
                {
                    f_itemInfo_ps->runtime_s.mngmtState_e = APPSDM_DIAG_ITEM_STATUS_ON;
                }
            }
            //----- Diagnostic is in ON State -----//
            else if(f_itemInfo_ps->runtime_s.mngmtState_e == APPSDM_DIAG_ITEM_STATUS_ON)
            {
                Ret_e = s_APPSDM_DiagStratMngmt(f_itemCfg_ps->diagStrat_e,
                                                APPSDM_DIAG_STRAT_INHIBIT_ON);

                if(((currentTime_u32 - f_itemInfo_ps->runtime_s.broadcastDelay_u32) > 
                        (t_uint32)APPSDM_BROADCAST_TIMEOUT)
                && (g_UserCallback_pcb != (t_cbAPPSDM_DiagEventBroadcast *)NULL_FUNCTION)
                && (f_itemCfg_ps->notifyUser_b == (t_bool)True))
                {
                    g_UserCallback_pcb( f_itemInfo_ps->desc_s.itemId_e,
                                        APPSDM_DIAG_ITEM_REPORT_FAIL,
                                        f_itemInfo_ps->desc_s.debugInfo1_u16,
                                        f_itemInfo_ps->desc_s.debugInfo2_u16);
                    
                    //----- Update Report Time -----//
                    f_itemInfo_ps->runtime_s.broadcastDelay_u32 = currentTime_u32;
                }
                //--- check if we haven't any report, to consider if OFF ----//
                if((currentTime_u32 - f_itemInfo_ps->desc_s.reportTime_u32) > f_itemCfg_ps->unactiveDelay_u32)
                {
                    f_itemInfo_ps->desc_s.reportstate_e = APPSDM_DIAG_ITEM_REPORT_PASS;
                }
            }
            
        }
        else // APPSDM_DIAG_ITEM_REPORT_PASS
        {
            //----- If the State is ON, call user one last time 
            //      to tell him that is done, else the diagnostic 
            //      is still in debouncer mode -----//
            if(f_itemInfo_ps->runtime_s.mngmtState_e == APPSDM_DIAG_ITEM_STATUS_ON )
            {
                //----- DeInhibit Function -----//
                Ret_e = s_APPSDM_DiagStratMngmt(f_itemCfg_ps->diagStrat_e,
                                            APPSDM_DIAG_STRAT_INHIBIT_OFF);

                //----- Call User -----//
                if((g_UserCallback_pcb != (t_cbAPPSDM_DiagEventBroadcast *)NULL_FUNCTION)
                && (f_itemCfg_ps->notifyUser_b == (t_bool)True))
                {
                    g_UserCallback_pcb( f_itemInfo_ps->desc_s.itemId_e,
                                        APPSDM_DIAG_ITEM_REPORT_PASS,
                                        f_itemInfo_ps->desc_s.debugInfo1_u16,
                                        f_itemInfo_ps->desc_s.debugInfo2_u16);    
                }
                
            }
            
            //----- Update Information -----//
            if(g_diagItemCnt_u8 > (t_uint8)0)
            {
                g_diagItemCnt_u8 -= (t_uint8)1;
            }

            f_itemInfo_ps->runtime_s.mngmtState_e = APPSDM_DIAG_ITEM_STATUS_OFF;
            f_itemInfo_ps->desc_s.debugInfo1_u16 = (t_uint16)0;
            f_itemInfo_ps->desc_s.debugInfo2_u16 = (t_uint16)0;
        }
    }

    return Ret_e;
}

/*********************************
 * s_APPSDM_DiagStratMngmt
 *********************************/
static t_eReturnCode s_APPSDM_DiagStratMngmt(t_eAPPSDM_DiagnosticStrat f_diagStrat_e,
                                             t_eAPPSDM_DiagStratOpe f_stratOpe_e)
{
    t_eReturnCode Ret_e;

    if((f_diagStrat_e >= APPSDM_DIAG_STRAT_NB)
    && (f_diagStrat_e != APPSDM_DIAG_STRAT_NONE))
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
//************************************************************************************
// End of File
//************************************************************************************

/**
 *
 *	@brief
 *	@note
 *
 *
 *	@param[in] 
 *	@param[out]
 *	 
 *
 *
 */
