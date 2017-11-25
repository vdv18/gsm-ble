/**
 * \author  Tilen Majerle
 * \email   tilen@majerle.eu
 * \website 
 * \license MIT
 * \brief   GSM config
 *	
\verbatim
   ----------------------------------------------------------------------
    Copyright (c) 2016 Tilen Majerle

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge,
    publish, distribute, sublicense, and/or sell copies of the Software, 
    and to permit persons to whom the Software is furnished to do so, 
    subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
    AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.
   ----------------------------------------------------------------------
\endverbatim
 */
#ifndef GSM_CONF_H
#define GSM_CONF_H 100

/**************************************************************************/
/**************************************************************************/
/**************************************************************************/
/*    Edit file name to gsm_config.h and edit values for your platform    */
/**************************************************************************/
/**************************************************************************/
/**************************************************************************/

/**
 * \defgroup CONFIG
 * \brief    GSM Config
 * \{
 */

/**
 * \brief  GSM receive buffer size in units of bytes for processing
 *
 * \note   Use as much as possible, but not less than 128 bytes.
 *         For faster CPU you may be allowed to use smaller buffer.
 */
#define GSM_BUFFER_SIZE                 512

/**
 * \brief  Enables (1) or disables (0) RTOS support (re-entrancy) for library.
 *
 *         When using RTOS, some additional configuration must be set, listed below.
 *
 * \note   This mode should be enabled only when processing function (GSM_Update) is called from separate thread.
 *         When everything is done in single thread regarding GSM, there is no need for this feature to be enabled.
 *
 * \note   When this mode is enabled, RTOS dependant locking system is required for thread synchronization.
 */
#define GSM_RTOS                        0

/**
 * \brief  RTOS sync object for mutex
 */
#define GSM_RTOS_SYNC_t                 osMutexDef_t

/**
 * \brief  Timeout in milliseconds for mutex to access API
 */
#define GSM_RTOS_TIMEOUT                180000

/**
 * \brief  Async data processing enabled (1) or disabled (0)
 *
 * \note   This feature has sense when in non-RTOS mode and you wanna process income data async (in interrupt).
 * 
 *         When this feature is enabled, you HAVE TO do processing (GSM_Update) in interrupt.
 */
#define GSM_ASYNC                       1

/**
 * \brief  Maximal SMS length in units of bytes
 */
#define GSM_SMS_MAX_LENGTH              160

/**
 * \brief  Maximal number of stored informations about received SMS at a time
 *
 *         When you are in other actions and you can't check SMS, 
 *         stack will save as many received SMS infos as possible (selected with this option)  
 */
#define GSM_MAX_RECEIVED_SMS_INFO       3

/**
 * \brief  Enables (1) or disables (0) functions for HTTP API features on GSM device
 *
 * \note   When disabled, there is no support for built-in HTTP processing on GSM module, 
 *         all HTTP related functions are disabled.
 */
#define GSM_HTTP                        1

/**
 * \brief  Enables (1) or disables (0) functions for FTP API features on GSM device
 *
 * \note   When disabled, there is no support for built-in FTP processing on GSM module, 
 *         all FTP related functions are disabled.
 */
#define GSM_FTP                         1

/**
 * \brief  Enables (1) or disables (0) functions for PHONEBOOK API features on GSM device
 *
 * \note   When disabled, there is no support for built-in PHONEBOOK processing on GSM module, 
 *         all PHONEBOOK related functions are disabled.
 */
#define GSM_PHONEBOOK                   1

/**
 * \brief  Enables (1) or disables (0) functions for CALL API features on GSM device
 *
 * \note   When disabled, there is no support for built-in CALL processing on GSM module, 
 *         all CALL related functions are disabled.
 *
 * \note   When CALL is received, there is no notification or event for user.
 */
#define GSM_CALL                        1

/**
 * \brief  Enables (1) or disables (0) functions for SMS API features on GSM device
 *
 * \note   When disabled, there is no support for built-in SMS processing on GSM module, 
 *         all SMS related functions are disabled.
 *
 * \note   When SMS is received, there is no notification or event for user.
 */
#define GSM_SMS                         1

/**
 * \}
 */

#endif
