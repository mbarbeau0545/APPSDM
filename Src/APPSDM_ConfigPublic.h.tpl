/*********************************************************************
 * @file        APPACT_ConfigPublic.h
 * @brief       Template_BriefDescription.
 * @note        TemplateDetailsDescription.\n
 *
 * @author      xxxxxx
 * @date        jj/mm/yyyy
 * @version     1.0
 */
  
#ifndef APPSDM_CONFIGPUBLIC_H_INCLUDED
#define APPSDM_CONFIGPUBLIC_H_INCLUDED

    // ********************************************************************
    // *                      Includes
    // ********************************************************************
    #include "TypeCommon.h"
    // ********************************************************************
    // *                      Defines
    // ********************************************************************
    #define APPSDM_BROADCAST_TIMEOUT ((t_uint32)1000) /**< Every Second  */
    // ********************************************************************
    // *                      Types
    // ********************************************************************
    /* CAUTION : Automatic generated code section for Enum: Start */
    /**
    * @brief Enum Diagnostic Item
    */
    typedef enum ____t_eAPPSDM_DiagnosticItem
    {
        APPSDM_DIAG_ITEM_FMK_CFG_ERROR = 0x00,// None
        APPSDM_DIAG_ITEM_FMK_CPU_TEMP_OUT_OF_RANGE,// None
        APPSDM_DIAG_ITEM_FMK_SUPPLY_VOLTAGE_OUT_OF_RANGE,// None
        APPSDM_DIAG_ITEM_FMK_FDCAN_OPE_ERROR,// None
        APPSDM_DIAG_ITEM_FMK_SRL_OPE_ERROR,// None
        APPSDM_DIAG_ITEM_FMK_HRT_OPE_ERROR,// None
        APPSDM_DIAG_ITEM_FMK_TIM_OPE_ERROR,// None
        APPSDM_DIAG_ITEM_FMK_CDA_OPE_ERROR,// None
        APPSDM_DIAG_ITEM_FMK_CPU_OPE_ERROR,// None
        APPSDM_DIAG_ITEM_APP_CYCLIC_TIMEOUT,// None
        APPSDM_DIAG_ITEM_APP_FASTTASK_TIMEOUT,// None
        APPSDM_DIAG_ITEM_UDS_COM_ERROR,// None
        APPSDM_DIAG_ITEM_APPSIG_MSG_TIMEOUT,// None

        APPSDM_DIAG_ITEM_NB,
    } t_eAPPSDM_DiagnosticItem;
    /**
    * @brief Enum Diagnostic Strategy
    */
    typedef enum ____t_eAPPSDM_DiagnosticStrat
    {

        APPSDM_DIAG_STRAT_NB,

        APPSDM_DIAG_STRAT_NONE,
    } t_eAPPSDM_DiagnosticStrat;
    /* CAUTION : Automatic generated code section for Enum: End */
	//-----------------------------ENUM TYPES-----------------------------//

	/* CAUTION : Automatic generated code section for Structure: Start */

	/* CAUTION : Automatic generated code section for Structure: End */
	//-----------------------------STRUCT TYPES---------------------------//
    typedef struct
    {
        t_uint16 DebuncCnt_u16;
        t_uint32 unactiveDelay_u32;
        t_bool notifyUser_b;
        t_bool logItem_b;
        t_eAPPSDM_DiagnosticStrat diagStrat_e;
    }t_sAPPSM_DiagItemCfg;
	/* CAUTION : Automatic generated code section : Start */

	/* CAUTION : Automatic generated code section : End */
	//-----------------------------TYPEDEF TYPES---------------------------//
    // ********************************************************************
    // *                      Prototypes
    // ********************************************************************
    
    // ********************************************************************
    // *                      Variables
    // ********************************************************************
    
    

    //********************************************************************************
    //                      Public functions - Prototyupes
    //********************************************************************************

#endif // APPSDM_CONFIGPUBLIC_H_INCLUDED           
//************************************************************************************
// End of File
//************************************************************************************

/**
 *
 *	@brief
 *	@note   
 *
 *
 *	@params[in] 
 *	@params[out]
 *	 
 *
 *
 */
