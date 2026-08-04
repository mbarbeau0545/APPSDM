
/*********************************************************************
 * @file        File.h
 * @brief       Template_BriefDescription.
 * @note        TemplateDetailsDescription.\n
 *
 * @author      xxxxxx
 * @date        jj/mm/yyyy
 * @version     1.0
 */
  
#ifndef APPSDM_H_INCLUDED 
#define APPSDM_H_INCLUDED 





    // ********************************************************************
    // *                      Includes
    // ********************************************************************
    #include "APP_CFG/ConfigFiles/APPSDM_ConfigPublic.h"
    // ********************************************************************
    // *                      Defines
    // ********************************************************************
    
    // ********************************************************************
    // *                      Types
    // ********************************************************************
    //-----------------------------ENUM TYPES-----------------------------//
    typedef enum 
    {
        APPSDM_DIAG_ITEM_REPORT_PASS = 0X00,
        APPSDM_DIAG_ITEM_REPORT_FAIL,

        APPSDM_DIAG_ITEM_STATE_NB
    } t_eAPPSDM_DiagnosticReport;
    //-----------------------------STRUCT TYPES-----------------------------//

    //-----------------------------TYPEDEF TYPES-----------------------------//
    typedef void (t_cbAPPSDM_DiagEventBroadcast)(   t_eAPPSDM_DiagnosticItem f_item_e,
                                                    t_eAPPSDM_DiagnosticReport f_reportState_e,
                                                    t_uint16 f_debugInfo1_u16,
                                                    t_uint16 f_debugInfo2_u16);
    // ********************************************************************
    // *                      Prototypes
    // ********************************************************************
        
    // ********************************************************************
    // *                      Variables
    // ********************************************************************


    //********************************************************************************
    //                      Public functions - Prototyupes
    //********************************************************************************
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
    /** @brief Reports a diagnostic event and broadcasts it to registered listeners.
     * @param[in] f_item_e Diagnostic item to update.
     * @param[in] f_reportState_e New diagnostic report state.
     * @param[in] f_debugInfo1_u16 First implementation-specific debug value.
     * @param[in] f_debugInfo2_u16 Second implementation-specific debug value. */
   void APPSDM_ReportDiagEvnt(  t_eAPPSDM_DiagnosticItem f_item_e,
                                t_eAPPSDM_DiagnosticReport f_reportState_e,
                                t_uint16 f_debugInfo1_u16,
                                t_uint16 f_debugInfo2_u16);
    /** @brief Gets the current report state of a diagnostic item.
     * @param[in] f_item_e Diagnostic item to query.
     * @param[out] f_state_pe Destination for the report state.
     * @return Status of the query. */
    t_eReturnCode APPSDM_GetDiagStatus( t_eAPPSDM_DiagnosticItem f_item_e,
                                        t_eAPPSDM_DiagnosticReport * f_state_pe); 
    /** @brief Resets all diagnostic events to their initial state. @return Status of the reset. */
   t_eReturnCode APPSDM_ResetDiagEvnt(void);
    /** @brief Registers a diagnostic-event broadcast callback.
     * @param[in] f_evntCallback_pcb Callback to invoke when an event is reported.
     * @return Registration status. */
   t_eReturnCode APPSDM_AddCallbackEvnt(t_cbAPPSDM_DiagEventBroadcast * f_evntCallback_pcb);                   
#endif // APPSDM_H_INCLUDED           
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
