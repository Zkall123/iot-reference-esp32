/*
 * ESP32-C3 Featured FreeRTOS IoT Integration V202204.00
 * Copyright (C) 2022 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

/*
 * This file demonstrates numerous tasks all of which use the core-MQTT Agent API
 * to send unique MQTT payloads to unique topics over the same MQTT connection
 * to the same coreMQTT-Agent.
 *
 * Each created task is a unique instance of the task implemented by
 * prvSubscribePublishUnsubscribeTask().  prvSubscribePublishUnsubscribeTask()
 * subscribes to a topic, publishes a message to the same
 * topic, receives the message, then unsubscribes from the topic in a loop.
 * The command context sent to MQTTAgent_Publish(), MQTTAgent_Subscribe(), and
 * MQTTAgent_Unsubscribe contains a unique number that is sent back to the task
 * as a task notification from the callback function that executes when the
 * operations are acknowledged (or just sent in the case of QoS 0).  The
 * task checks the number it receives from the callback equals the number it
 * previously set in the command context before printing out either a success
 * or failure message.
 */

/* Includes *******************************************************************/

/* Standard includes. */
#include <string.h>
#include <stdio.h>
#include <assert.h>

/* FreeRTOS includes. */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

/* ESP-IDF includes. */
#include "esp_log.h"
#include "esp_event.h"
#include "sdkconfig.h"

/* coreMQTT library include. */
#include "core_mqtt.h"

/* coreMQTT-Agent include. */
#include "core_mqtt_agent.h"

/* coreMQTT-Agent network manager include. */
#include "core_mqtt_agent_manager.h"
#include "core_mqtt_agent_manager_events.h"

/* Subscription manager include. */
#include "subscription_manager.h"

/* Public functions include. */
#include "sub_pub_unsub_demo.h"

/* Demo task configurations include. */
#include "sub_pub_unsub_demo_config.h"

//LUDO Includes
#include "extras/NFC.h"
#include "extras/ledStrip.h"
#include "extras/sntpTime.h"
#include "extras/TasksCommon.h"
#include "lan.h"

//Json Stuff
#include "core_json.h"
#include "extras/Json.h"

/* Preprocessor definitions ***************************************************/

/* coreMQTT-Agent event group bit definitions */
#define CORE_MQTT_AGENT_CONNECTED_BIT              ( 1 << 0 )
#define CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT    ( 1 << 1 )

/* MQTT event group bit definitions. */
#define MQTT_INCOMING_PUBLISH_RECEIVED_BIT         ( 1 << 0 )
#define MQTT_PUBLISH_COMMAND_COMPLETED_BIT         ( 1 << 1 )
#define MQTT_SUBSCRIBE_COMMAND_COMPLETED_BIT       ( 1 << 2 )
#define MQTT_UNSUBSCRIBE_COMMAND_COMPLETED_BIT     ( 1 << 3 )

/* Struct definitions *********************************************************/

/**
 * @brief Defines the structure to use as the incoming publish callback context
 * when data from a subscribed topic is received.
 */
typedef struct IncomingPublishCallbackContext
{
    EventGroupHandle_t xMqttEventGroup;
    char pcIncomingPublish[ subpubunsubconfigSTRING_BUFFER_LENGTH ];
} IncomingPublishCallbackContext_t;

/**
 * @brief Defines the structure to use as the command callback context in this
 * demo.
 */
struct MQTTAgentCommandContext
{
    MQTTStatus_t xReturnStatus;
    EventGroupHandle_t xMqttEventGroup;
    IncomingPublishCallbackContext_t * pxIncomingPublishCallbackContext;
    void * pArgs;
};

/**
 * @brief Parameters for this task.
 */
struct DemoParams
{
    uint32_t ulTaskNumber;
};

/* Global variables ***********************************************************/

/**
 * @brief Logging tag for ESP-IDF logging functions.
 */
static const char * TAG = "sub_pub_unsub_demo";

/**
 * @brief Static handle used for MQTT agent context.
 */
extern MQTTAgentContext_t xGlobalMqttAgentContext;

/**
 * @brief The buffer to hold the topic filter. The topic is generated at runtime
 * by adding the task names.
 *
 * @note The topic strings must persist until unsubscribed.
 
static char topicBuf[ subpubunsubconfigNUM_TASKS_TO_CREATE ][ subpubunsubconfigSTRING_BUFFER_LENGTH ];
*/
/**
 * @brief The event group used to manage coreMQTT-Agent events.
 */
static EventGroupHandle_t xNetworkEventGroup;

/**
 * @brief The semaphore used to lock access to ulMessageID to eliminate a race
 * condition in which multiple tasks try to increment/get ulMessageID.
 */
static SemaphoreHandle_t xMessageIdSemaphore;

/**
 * @brief The message ID for the next message sent by this demo.
 */
static uint32_t ulMessageId = 0;

/* Static function declarations ***********************************************/

/**
 * @brief ESP Event Loop library handler for coreMQTT-Agent events.
 *
 * This handles events defined in core_mqtt_agent_events.h.
 */
static void prvCoreMqttAgentEventHandler( void * pvHandlerArg,
                                          esp_event_base_t xEventBase,
                                          int32_t lEventId,
                                          void * pvEventData );

/**
 * @brief Passed into MQTTAgent_Subscribe() as the callback to execute when the
 * broker ACKs the SUBSCRIBE message.  Its implementation sends a notification
 * to the task that called MQTTAgent_Subscribe() to let the task know the
 * SUBSCRIBE operation completed.  It also sets the xReturnStatus of the
 * structure passed in as the command's context to the value of the
 * xReturnStatus parameter - which enables the task to check the status of the
 * operation.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pxCommandContext Context of the initial command.
 * @param[in].xReturnStatus The result of the command.
 */
static void prvSubscribeCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                         MQTTAgentReturnInfo_t * pxReturnInfo );

static void prvUnsubscribeCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                           MQTTAgentReturnInfo_t * pxReturnInfo );

/**
 * @brief Passed into MQTTAgent_Publish() as the callback to execute when the
 * broker ACKs the PUBLISH message.  Its implementation sends a notification
 * to the task that called MQTTAgent_Publish() to let the task know the
 * PUBLISH operation completed.  It also sets the xReturnStatus of the
 * structure passed in as the command's context to the value of the
 * xReturnStatus parameter - which enables the task to check the status of the
 * operation.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pxCommandContext Context of the initial command.
 * @param[in].xReturnStatus The result of the command.
 */
static void prvPublishCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                       MQTTAgentReturnInfo_t * pxReturnInfo );

/**
 * @brief Called by the task to wait for event from a callback function
 * after the task first executes either MQTTAgent_Publish()* or
 * MQTTAgent_Subscribe().
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] xMqttEventGroup Event group used for MQTT events.
 * @param[in] uxBitsToWaitFor Event to wait for.
 *
 * @return Received event.
 */
static EventBits_t prvWaitForEvent( EventGroupHandle_t xMqttEventGroup,
                                    EventBits_t uxBitsToWaitFor );

/**
 * @brief Passed into MQTTAgent_Subscribe() as the callback to execute when
 * there is an incoming publish on the topic being subscribed to.  Its
 * implementation just logs information about the incoming publish including
 * the publish messages source topic and payload.
 *
 * See https://freertos.org/mqtt/mqtt-agent-demo.html#example_mqtt_api_call
 *
 * @param[in] pvIncomingPublishCallbackContext Context of the initial command.
 * @param[in] pxPublishInfo Deserialized publish.
 */
static void prvIncomingPublishCallback( void * pvIncomingPublishCallbackContext,
                                        MQTTPublishInfo_t * pxPublishInfo );

/**
 * @brief Subscribe to the topic the demo task will also publish to - that
 * results in all outgoing publishes being published back to the task
 * (effectively echoed back).
 *
 * @param[in] pxIncomingPublishCallbackContext The callback context used when
 * data is received from pcTopicFilter.
 * @param[in] xQoS The quality of service (QoS) to use.  Can be zero or one
 * for all MQTT brokers.  Can also be QoS2 if supported by the broker.  AWS IoT
 * does not support QoS2.
 * @param[in] pcTopicFilter Topic filter to subscribe to.
 * @param[in] xMqttEventGroup Event group used for MQTT events.
 */
static void prvSubscribeToTopic( IncomingPublishCallbackContext_t * pxIncomingPublishCallbackContext,
                                 MQTTQoS_t xQoS,
                                 char * pcTopicFilter,
                                 EventGroupHandle_t xMqttEventGroup );

/**
 * @brief Unsubscribe to the topic the demo task will also publish to.
 *
 * @param[in] xQoS The quality of service (QoS) to use.  Can be zero or one
 * for all MQTT brokers.  Can also be QoS2 if supported by the broker.  AWS IoT
 * does not support QoS2.
 * @param[in] pcTopicFilter Topic filter to unsubscribe from.
 * @param[in] xMqttEventGroup Event group used for MQTT events.
 */
static void prvUnsubscribeToTopic( MQTTQoS_t xQoS,
                                   char * pcTopicFilter,
                                   EventGroupHandle_t xMqttEventGroup );

/**
 * @brief The function that implements the task demonstrated by this file.
 
static void prvSubscribePublishUnsubscribeTask( void * pvParameters );
*/
/* Static function definitions ************************************************/

static void prvCoreMqttAgentEventHandler( void * pvHandlerArg,
                                          esp_event_base_t xEventBase,
                                          int32_t lEventId,
                                          void * pvEventData )
{
    ( void ) pvHandlerArg;
    ( void ) xEventBase;
    ( void ) pvEventData;

    switch( lEventId )
    {
        case CORE_MQTT_AGENT_CONNECTED_EVENT:
            ESP_LOGI( TAG,
                      "coreMQTT-Agent connected." );
            xEventGroupSetBits( xNetworkEventGroup,
                                CORE_MQTT_AGENT_CONNECTED_BIT );
            sntpTimeTaskStart();
            break;

        case CORE_MQTT_AGENT_DISCONNECTED_EVENT:
            ESP_LOGI( TAG,
                      "coreMQTT-Agent disconnected. Preventing coreMQTT-Agent "
                      "commands from being enqueued." );
            xEventGroupClearBits( xNetworkEventGroup,
                                  CORE_MQTT_AGENT_CONNECTED_BIT );
            break;

        case CORE_MQTT_AGENT_OTA_STARTED_EVENT:
            ESP_LOGI( TAG,
                      "OTA started. Preventing coreMQTT-Agent commands from "
                      "being enqueued." );
            xEventGroupClearBits( xNetworkEventGroup,
                                  CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT );
            break;

        case CORE_MQTT_AGENT_OTA_STOPPED_EVENT:
            ESP_LOGI( TAG,
                      "OTA stopped. No longer preventing coreMQTT-Agent "
                      "commands from being enqueued." );
            xEventGroupSetBits( xNetworkEventGroup,
                                CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT );
            break;

        default:
            ESP_LOGE( TAG,
                      "coreMQTT-Agent event handler received unexpected event: %" PRIu32 "",
                      lEventId );
            break;
    }
}

static void prvPublishCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                       MQTTAgentReturnInfo_t * pxReturnInfo )
{
    /* Store the result in the application defined context so the task that
     * initiated the publish can check the operation's status. */
    pxCommandContext->xReturnStatus = pxReturnInfo->returnCode;

    if( pxCommandContext->xMqttEventGroup != NULL )
    {
        xEventGroupSetBits( pxCommandContext->xMqttEventGroup,
                            MQTT_PUBLISH_COMMAND_COMPLETED_BIT );
    }
}

static void prvSubscribeCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                         MQTTAgentReturnInfo_t * pxReturnInfo )
{
    bool xSubscriptionAdded = false;
    MQTTAgentSubscribeArgs_t * pxSubscribeArgs = ( MQTTAgentSubscribeArgs_t * ) pxCommandContext->pArgs;

    /* Store the result in the application defined context so the task that
     * initiated the subscribe can check the operation's status. */
    pxCommandContext->xReturnStatus = pxReturnInfo->returnCode;

    /* Check if the subscribe operation is a success. */
    if( pxReturnInfo->returnCode == MQTTSuccess )
    {
        /* Add subscription so that incoming publishes are routed to the application
         * callback. */
        xSubscriptionAdded = addSubscription( ( SubscriptionElement_t * ) xGlobalMqttAgentContext.pIncomingCallbackContext,
                                              pxSubscribeArgs->pSubscribeInfo->pTopicFilter,
                                              pxSubscribeArgs->pSubscribeInfo->topicFilterLength,
                                              prvIncomingPublishCallback,
                                              ( void * ) ( pxCommandContext->pxIncomingPublishCallbackContext ) );

        if( xSubscriptionAdded == false )
        {
            ESP_LOGE( TAG,
                      "Failed to register an incoming publish callback for topic %.*s.",
                      pxSubscribeArgs->pSubscribeInfo->topicFilterLength,
                      pxSubscribeArgs->pSubscribeInfo->pTopicFilter );
        }
    }

    if( pxCommandContext->xMqttEventGroup != NULL )
    {
        xEventGroupSetBits( pxCommandContext->xMqttEventGroup,
                            MQTT_SUBSCRIBE_COMMAND_COMPLETED_BIT );
    }
}

static void prvUnsubscribeCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
                                           MQTTAgentReturnInfo_t * pxReturnInfo )
{
    MQTTAgentSubscribeArgs_t * pxUnsubscribeArgs = ( MQTTAgentSubscribeArgs_t * ) pxCommandContext->pArgs;

    /* Store the result in the application defined context so the task that
     * initiated the subscribe can check the operation's status. */
    pxCommandContext->xReturnStatus = pxReturnInfo->returnCode;

    /* Check if the unsubscribe operation is a success. */
    if( pxReturnInfo->returnCode == MQTTSuccess )
    {
        /* Remove subscription from subscription manager. */
        removeSubscription( ( SubscriptionElement_t * ) xGlobalMqttAgentContext.pIncomingCallbackContext,
                            pxUnsubscribeArgs->pSubscribeInfo->pTopicFilter,
                            pxUnsubscribeArgs->pSubscribeInfo->topicFilterLength );
    }

    if( pxCommandContext->xMqttEventGroup != NULL )
    {
        xEventGroupSetBits( pxCommandContext->xMqttEventGroup,
                            MQTT_UNSUBSCRIBE_COMMAND_COMPLETED_BIT );
    }
}

static EventBits_t prvWaitForEvent( EventGroupHandle_t xMqttEventGroup,
                                    EventBits_t uxBitsToWaitFor )
{
    EventBits_t xReturn;

    xReturn = xEventGroupWaitBits( xMqttEventGroup,
                                   uxBitsToWaitFor,
                                   pdTRUE, /* xClearOnExit. */
                                   pdTRUE, /* xWaitForAllBits. */
                                   portMAX_DELAY );
    return xReturn;
}

static void prvIncomingPublishCallback( void * pvIncomingPublishCallbackContext,
                                        MQTTPublishInfo_t * pxPublishInfo )
{
    IncomingPublishCallbackContext_t * pxIncomingPublishCallbackContext = ( IncomingPublishCallbackContext_t * ) pvIncomingPublishCallbackContext;

    /* Create a message that contains the incoming MQTT payload to the logger,
     * terminating the string first. */
    if( pxPublishInfo->payloadLength < subpubunsubconfigSTRING_BUFFER_LENGTH )
    {
        memcpy( ( void * ) ( pxIncomingPublishCallbackContext->pcIncomingPublish ),
                pxPublishInfo->pPayload,
                pxPublishInfo->payloadLength );

        ( pxIncomingPublishCallbackContext->pcIncomingPublish )[ pxPublishInfo->payloadLength ] = 0x00;
    }
    else
    {
        memcpy( ( void * ) ( pxIncomingPublishCallbackContext->pcIncomingPublish ),
                pxPublishInfo->pPayload,
                subpubunsubconfigSTRING_BUFFER_LENGTH );

        ( pxIncomingPublishCallbackContext->pcIncomingPublish )[ subpubunsubconfigSTRING_BUFFER_LENGTH - 1 ] = 0x00;
    }

    xEventGroupSetBits( pxIncomingPublishCallbackContext->xMqttEventGroup,
                        MQTT_INCOMING_PUBLISH_RECEIVED_BIT );
}

static void prvPublishToTopic( MQTTQoS_t xQoS,
                               char * pcTopicName,
                               char * pcPayload,
                               EventGroupHandle_t xMqttEventGroup )
{
    uint32_t ulPublishMessageId = 0;

    MQTTStatus_t xCommandAdded;
    EventBits_t xReceivedEvent = 0;

    MQTTPublishInfo_t xPublishInfo = { 0 };

    MQTTAgentCommandContext_t xCommandContext = { 0 };
    MQTTAgentCommandInfo_t xCommandParams = { 0 };

    xTaskNotifyStateClear( NULL );

    /* Create a unique number for the publish that is about to be sent.
     * This number is used in the command context and is sent back to this task
     * as a notification in the callback that's executed upon receipt of the
     * publish from coreMQTT-Agent.
     * That way this task can match an acknowledgment to the message being sent.
     */
    xSemaphoreTake( xMessageIdSemaphore, portMAX_DELAY );
    {
        ++ulMessageId;
        ulPublishMessageId = ulMessageId;
    }
    xSemaphoreGive( xMessageIdSemaphore );

    /* Configure the publish operation. The topic name string must persist for
     * duration of publish! */
    xPublishInfo.qos = xQoS;
    xPublishInfo.pTopicName = pcTopicName;
    xPublishInfo.topicNameLength = ( uint16_t ) strlen( pcTopicName );
    xPublishInfo.pPayload = pcPayload;
    xPublishInfo.payloadLength = ( uint16_t ) strlen( pcPayload );

    /* Complete an application defined context associated with this publish
     * message.
     * This gets updated in the callback function so the variable must persist
     * until the callback executes. */
    xCommandContext.xMqttEventGroup = xMqttEventGroup;

    xCommandParams.blockTimeMs = subpubunsubconfigMAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvPublishCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = &xCommandContext;

    do
    {
        /* Wait for coreMQTT-Agent task to have working network connection and
         * not be performing an OTA update. */
        xEventGroupWaitBits( xNetworkEventGroup,
                             CORE_MQTT_AGENT_CONNECTED_BIT | CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT,
                             pdFALSE,
                             pdTRUE,
                             portMAX_DELAY );

        /*ESP_LOGI( TAG,
                  "Task \"%s\" sending publish request to coreMQTT-Agent with message \"%s\" on topic \"%s\" with ID %" PRIu32 ".",
                  pcTaskGetName( NULL ),
                  pcPayload,
                  pcTopicName,
                  ulPublishMessageId );*/

        xCommandAdded = MQTTAgent_Publish( &xGlobalMqttAgentContext,
                                           &xPublishInfo,
                                           &xCommandParams );

        if( xCommandAdded == MQTTSuccess )
        {
            /* For QoS 1 and 2, wait for the publish acknowledgment.  For QoS0,
             * wait for the publish to be sent. */
            /*ESP_LOGI( TAG,
                      "Task \"%s\" waiting for publish %" PRIu32 " to complete.",
                      pcTaskGetName( NULL ),
                      ulPublishMessageId );*/

            xReceivedEvent = prvWaitForEvent( xMqttEventGroup,
                                              MQTT_PUBLISH_COMMAND_COMPLETED_BIT );
        }
        else
        {
            ESP_LOGE( TAG,
                      "Failed to enqueue publish command. Error code=%s",
                      MQTT_Status_strerror( xCommandAdded ) );
        }

        /* Check all ways the status was passed back just for demonstration
         * purposes. */
        if( ( ( xReceivedEvent & MQTT_PUBLISH_COMMAND_COMPLETED_BIT ) == 0 ) ||
            ( xCommandContext.xReturnStatus != MQTTSuccess ) )
        {
            ESP_LOGW( TAG,
                      "Error or timed out waiting for ack for publish message %" PRIu32 ". Re-attempting publish.",
                      ulPublishMessageId );
        }
        else
        {
            /*ESP_LOGI( TAG,
                      "Publish %" PRIu32 " succeeded for task \"%s\".",
                      ulPublishMessageId,
                      pcTaskGetName( NULL ) );*/
        }
    } while( ( xReceivedEvent & MQTT_PUBLISH_COMMAND_COMPLETED_BIT ) == 0 ||
             ( xCommandContext.xReturnStatus != MQTTSuccess ) );
}

static void prvSubscribeToTopic( IncomingPublishCallbackContext_t * pxIncomingPublishCallbackContext,
                                 MQTTQoS_t xQoS,
                                 char * pcTopicFilter,
                                 EventGroupHandle_t xMqttEventGroup )
{
    uint32_t ulSubscribeMessageId;

    MQTTStatus_t xCommandAdded;
    EventBits_t xReceivedEvent = 0;

    MQTTAgentSubscribeArgs_t xSubscribeArgs = { 0 };
    MQTTSubscribeInfo_t xSubscribeInfo = { 0 };

    MQTTAgentCommandContext_t xCommandContext = { 0 };
    MQTTAgentCommandInfo_t xCommandParams = { 0 };

    xTaskNotifyStateClear( NULL );

    /* Create a unique number for the subscribe that is about to be sent.
     * This number is used in the command context and is sent back to this task
     * as a notification in the callback that's executed upon receipt of the
     * publish from coreMQTT-Agent.
     * That way this task can match an acknowledgment to the message being sent.
     */
    xSemaphoreTake( xMessageIdSemaphore, portMAX_DELAY );
    {
        ++ulMessageId;
        ulSubscribeMessageId = ulMessageId;
    }
    xSemaphoreGive( xMessageIdSemaphore );

    /* Configure the subscribe operation.  The topic string must persist for
     * duration of subscription! */
    xSubscribeInfo.qos = xQoS;
    xSubscribeInfo.pTopicFilter = pcTopicFilter;
    xSubscribeInfo.topicFilterLength = ( uint16_t ) strlen( pcTopicFilter );

    xSubscribeArgs.pSubscribeInfo = &xSubscribeInfo;
    xSubscribeArgs.numSubscriptions = 1;

    /* Complete an application defined context associated with this subscribe
     * message.
     * This gets updated in the callback function so the variable must persist
     * until the callback executes. */
    xCommandContext.xMqttEventGroup = xMqttEventGroup;
    xCommandContext.pxIncomingPublishCallbackContext = pxIncomingPublishCallbackContext;
    xCommandContext.pArgs = ( void * ) &xSubscribeArgs;

    xCommandParams.blockTimeMs = subpubunsubconfigMAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvSubscribeCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = ( void * ) &xCommandContext;

    do
    {
        /* Wait for coreMQTT-Agent task to have working network connection and
         * not be performing an OTA update. */
        xEventGroupWaitBits( xNetworkEventGroup,
                             CORE_MQTT_AGENT_CONNECTED_BIT | CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT,
                             pdFALSE,
                             pdTRUE,
                             portMAX_DELAY );

        /*ESP_LOGI( TAG,
                  "Task \"%s\" sending subscribe request to coreMQTT-Agent for topic filter: %s with id %" PRIu32 "",
                  pcTaskGetName( NULL ),
                  pcTopicFilter,
                  ulSubscribeMessageId );*/

        xCommandAdded = MQTTAgent_Subscribe( &xGlobalMqttAgentContext,
                                             &xSubscribeArgs,
                                             &xCommandParams );

        if( xCommandAdded == MQTTSuccess )
        {
            /* For QoS 1 and 2, wait for the subscription acknowledgment. For QoS0,
             * wait for the subscribe to be sent. */
            xReceivedEvent = prvWaitForEvent( xMqttEventGroup,
                                              MQTT_SUBSCRIBE_COMMAND_COMPLETED_BIT );
        }
        else
        {
            ESP_LOGE( TAG,
                      "Failed to enqueue subscribe command. Error code=%s",
                      MQTT_Status_strerror( xCommandAdded ) );
        }

        /* Check all ways the status was passed back just for demonstration
         * purposes. */
        if( ( ( xReceivedEvent & MQTT_SUBSCRIBE_COMMAND_COMPLETED_BIT ) == 0 ) ||
            ( xCommandContext.xReturnStatus != MQTTSuccess ) )
        {
            ESP_LOGW( TAG,
                      "Error or timed out waiting for ack to subscribe message %" PRIu32 ". Re-attempting subscribe.",
                      ulSubscribeMessageId );
        }
        else
        {
            /*ESP_LOGI( TAG,
                      "Subscribe %" PRIu32 " for topic filter %s succeeded for task \"%s\".",
                      ulSubscribeMessageId,
                      pcTopicFilter,
                      pcTaskGetName( NULL ) );*/
        }
    } while( ( ( xReceivedEvent & MQTT_SUBSCRIBE_COMMAND_COMPLETED_BIT ) == 0 ) ||
             ( xCommandContext.xReturnStatus != MQTTSuccess ) );
}

static void prvUnsubscribeToTopic( MQTTQoS_t xQoS,
                                   char * pcTopicFilter,
                                   EventGroupHandle_t xMqttEventGroup )
{
    uint32_t ulUnsubscribeMessageId;

    MQTTStatus_t xCommandAdded;
    EventBits_t xReceivedEvent = 0;

    MQTTAgentSubscribeArgs_t xUnsubscribeArgs = { 0 };
    MQTTSubscribeInfo_t xUnsubscribeInfo = { 0 };

    MQTTAgentCommandContext_t xCommandContext = { 0 };
    MQTTAgentCommandInfo_t xCommandParams = { 0 };

    xTaskNotifyStateClear( NULL );

    /* Create a unique number for the subscribe that is about to be sent.
     * This number is used in the command context and is sent back to this task
     * as a notification in the callback that's executed upon receipt of the
     * publish from coreMQTT-Agent.
     * That way this task can match an acknowledgment to the message being sent.
     */
    xSemaphoreTake( xMessageIdSemaphore, portMAX_DELAY );
    {
        ++ulMessageId;
        ulUnsubscribeMessageId = ulMessageId;
    }
    xSemaphoreGive( xMessageIdSemaphore );

    /* Configure the subscribe operation.  The topic string must persist for
     * duration of subscription! */
    xUnsubscribeInfo.qos = xQoS;
    xUnsubscribeInfo.pTopicFilter = pcTopicFilter;
    xUnsubscribeInfo.topicFilterLength = ( uint16_t ) strlen( pcTopicFilter );

    xUnsubscribeArgs.pSubscribeInfo = &xUnsubscribeInfo;
    xUnsubscribeArgs.numSubscriptions = 1;

    /* Complete an application defined context associated with this subscribe
     * message.
     * This gets updated in the callback function so the variable must persist
     * until the callback executes. */
    xCommandContext.xMqttEventGroup = xMqttEventGroup;
    xCommandContext.pArgs = ( void * ) &xUnsubscribeArgs;

    xCommandParams.blockTimeMs = subpubunsubconfigMAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvUnsubscribeCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = ( void * ) &xCommandContext;

    do
    {
        /* Wait for coreMQTT-Agent task to have working network connection and
         * not be performing an OTA update. */
        xEventGroupWaitBits( xNetworkEventGroup,
                             CORE_MQTT_AGENT_CONNECTED_BIT | CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT,
                             pdFALSE,
                             pdTRUE,
                             portMAX_DELAY );
        /*ESP_LOGI( TAG,
                  "Task \"%s\" sending unsubscribe request to coreMQTT-Agent for topic filter: %s with id %" PRIu32 "",
                  pcTaskGetName( NULL ),
                  pcTopicFilter,
                  ulUnsubscribeMessageId );*/

        xCommandAdded = MQTTAgent_Unsubscribe( &xGlobalMqttAgentContext,
                                               &xUnsubscribeArgs,
                                               &xCommandParams );

        if( xCommandAdded == MQTTSuccess )
        {
            /* For QoS 1 and 2, wait for the subscription acknowledgment.  For QoS0,
             * wait for the subscribe to be sent. */
            xReceivedEvent = prvWaitForEvent( xMqttEventGroup,
                                              MQTT_UNSUBSCRIBE_COMMAND_COMPLETED_BIT );
        }
        else
        {
            ESP_LOGE( TAG,
                      "Failed to enqueue unsubscribe command. Error code=%s",
                      MQTT_Status_strerror( xCommandAdded ) );
        }

        /* Check all ways the status was passed back just for demonstration
         * purposes. */
        if( ( ( xReceivedEvent & MQTT_UNSUBSCRIBE_COMMAND_COMPLETED_BIT ) == 0 ) ||
            ( xCommandContext.xReturnStatus != MQTTSuccess ) )
        {
            ESP_LOGW( TAG,
                      "Error or timed out waiting for ack to unsubscribe message %" PRIu32 ". Re-attempting subscribe.",
                      ulUnsubscribeMessageId );
        }
        else
        {
            /*ESP_LOGI( TAG,
                      "Unsubscribe %" PRIu32 " for topic filter %s succeeded for task \"%s\".",
                      ulUnsubscribeMessageId,
                      pcTopicFilter,
                      pcTaskGetName( NULL ) );*/
        }
    } while( ( ( xReceivedEvent & MQTT_UNSUBSCRIBE_COMMAND_COMPLETED_BIT ) == 0 ) ||
             ( xCommandContext.xReturnStatus != MQTTSuccess ) );
}

static void prvLudoPublishToTopic( MQTTQoS_t xQoS,
                               char * pcTopicName,
                               char * pcPayload,
                               EventGroupHandle_t xMqttEventGroup )
{
    uint32_t ulPublishMessageId = 0;

    MQTTStatus_t xCommandAdded;

    MQTTPublishInfo_t xPublishInfo = { 0 };

    MQTTAgentCommandContext_t xCommandContext = { 0 };
    MQTTAgentCommandInfo_t xCommandParams = { 0 };

    // Unique Message ID
    xSemaphoreTake( xMessageIdSemaphore, portMAX_DELAY );
    {
        ++ulMessageId;
        ulPublishMessageId = ulMessageId;
    }
    xSemaphoreGive( xMessageIdSemaphore );

    // Set up the publish parameters
    xPublishInfo.qos = xQoS;
    xPublishInfo.pTopicName = pcTopicName;
    xPublishInfo.topicNameLength = ( uint16_t ) strlen( pcTopicName );
    xPublishInfo.pPayload = pcPayload;
    xPublishInfo.payloadLength = ( uint16_t ) strlen( pcPayload );

    // Set up the command context and command info
    xCommandContext.xMqttEventGroup = xMqttEventGroup;

    xCommandParams.blockTimeMs = subpubunsubconfigMAX_COMMAND_SEND_BLOCK_TIME_MS;
    xCommandParams.cmdCompleteCallback = prvPublishCommandCallback;
    xCommandParams.pCmdCompleteCallbackContext = &xCommandContext;

    // Wait for coreMQTT-Agent to have network connection and be ready
    xEventGroupWaitBits( xNetworkEventGroup,
                         CORE_MQTT_AGENT_CONNECTED_BIT | CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT,
                         pdFALSE,
                         pdTRUE,
                         portMAX_DELAY );
    
    if ((xEventGroupGetBits(xNetworkEventGroup) & CORE_MQTT_AGENT_CONNECTED_BIT) == 0) {
        ESP_LOGE(TAG, "MQTT connection not established, cannot publish");
        return;
    }

    // Send the publish command to the coreMQTT-Agent
    xCommandAdded = MQTTAgent_Publish( &xGlobalMqttAgentContext,
                                       &xPublishInfo,
                                       &xCommandParams );

    if( xCommandAdded != MQTTSuccess )
    {
        ESP_LOGE( TAG,
                  "Failed to enqueue publish command. Error code=%s",
                  MQTT_Status_strerror( xCommandAdded ) );
    }
    else
    {
        ESP_LOGI( TAG,
                  "Task \"%s\" sent publish request with message \"%s\" on topic \"%s\" with ID %" PRIu32 ".",
                  pcTaskGetName( NULL ),
                  pcPayload,
                  pcTopicName,
                  ulPublishMessageId );
    }
}

//integer to set the payload Size
const int LudoPayloadSize = 1500;
//and for TopicSize
const int LudoTopicSize = 200;

static void ludoPublishToTopic(char* pcTopic, char pcPayload[LudoPayloadSize])
{
    EventGroupHandle_t xMqttEventGroup;
    IncomingPublishCallbackContext_t xIncomingPublishCallbackContext;

    MQTTQoS_t xQoS;

    xMqttEventGroup = xEventGroupCreate();
    xIncomingPublishCallbackContext.xMqttEventGroup = xMqttEventGroup;

    xQoS = ( MQTTQoS_t ) subpubunsubconfigQOS_LEVEL;

    //prvSubscribeToTopic(&xIncomingPublishCallbackContext, xQoS, pcTopic,xMqttEventGroup);

    prvPublishToTopic(xQoS, pcTopic, pcPayload, xMqttEventGroup);

    //prvWaitForEvent( xMqttEventGroup, MQTT_INCOMING_PUBLISH_RECEIVED_BIT );

    //ESP_LOGI( TAG, "Task \"%s\" received: %s", pcTaskGetName( NULL ), xIncomingPublishCallbackContext.pcIncomingPublish );

    //prvUnsubscribeToTopic( xQoS, pcTopic, xMqttEventGroup );

    vEventGroupDelete( xMqttEventGroup );

}

void prvSendUIDToAWS(char *UID)
{

    //ESP_LOGI(TAG, "Subscribing Access channel!");
    EventGroupHandle_t AccessMqttEventGroup;
    IncomingPublishCallbackContext_t AccessIncomingPublishCallbackContext;

    MQTTQoS_t xQoS;

    AccessMqttEventGroup = xEventGroupCreate();
    AccessIncomingPublishCallbackContext.xMqttEventGroup = AccessMqttEventGroup;

    xQoS = ( MQTTQoS_t ) subpubunsubconfigQOS_LEVEL;

    //Set the Topic to '/ThingName/Access/sub'
    char SetTopic[LudoTopicSize];
    snprintf(SetTopic ,LudoTopicSize, "device/access/%s/response", LanPrintMac());
    
    //Subscribing to channel!
    prvSubscribeToTopic(&AccessIncomingPublishCallbackContext, xQoS, SetTopic ,AccessMqttEventGroup);

    //ESP_LOGI(TAG, "UID: %s", UID);
     // Speicher für Payload dynamisch allokieren
    char *pcPayload = (char *) malloc(LudoPayloadSize * sizeof(char));
    if (pcPayload == NULL)
    {
        // Fehlerbehandlung für fehlgeschlagene Speicherzuweisung
        ESP_LOGE(TAG, "Failed to allocate memory for Payload");
        return;
    }
    char* JsonString = JsonAccessString(UID);
    snprintf( pcPayload, LudoPayloadSize, "%s", JsonString);

    //Set the Topic to '/ThingName/Access/pub'
    char pcTopic[LudoTopicSize];
    snprintf(pcTopic, LudoTopicSize, "device/access/%s/request", LanPrintMac());

    ludoPublishToTopic(pcTopic, pcPayload);

    prvWaitForEvent( AccessMqttEventGroup, MQTT_INCOMING_PUBLISH_RECEIVED_BIT );
    ESP_LOGW(TAG, "Access received: %s", AccessIncomingPublishCallbackContext.pcIncomingPublish );
    JsonParse(AccessIncomingPublishCallbackContext.pcIncomingPublish, "access");
    
    
    //Unsubscribe and delete Task!
    prvUnsubscribeToTopic(xQoS, SetTopic, AccessMqttEventGroup);
    vEventGroupDelete( AccessMqttEventGroup );

    //ESP_LOGI( TAG, "Task \"%s\" Sends UID to AWS. Waiting for next tag!.", pcTaskGetName( NULL ) );

    free(pcPayload);
}

static void ludoSettingsTask( void * pvParameters )
{
    ESP_LOGI(TAG, "Subscribing Setting channel!");
    EventGroupHandle_t SettingsMqttEventGroup;
    IncomingPublishCallbackContext_t SettingsIncomingPublishCallbackContext;

    MQTTQoS_t xQoS;

    SettingsMqttEventGroup = xEventGroupCreate();
    SettingsIncomingPublishCallbackContext.xMqttEventGroup = SettingsMqttEventGroup;


    xQoS = ( MQTTQoS_t ) subpubunsubconfigQOS_LEVEL;
    //Set the Topic to '/ThingName/Settings/pub'
    xEventGroupWaitBits( xNetworkEventGroup,
                             CORE_MQTT_AGENT_CONNECTED_BIT ,
                             pdFALSE,
                             pdTRUE,
                             portMAX_DELAY );
    char SetTopic[LudoTopicSize];
    snprintf(SetTopic ,LudoTopicSize, "device/settings/%s/response", LanPrintMac());
    
    //Subscribing to channel!
    prvSubscribeToTopic(&SettingsIncomingPublishCallbackContext, xQoS, SetTopic ,SettingsMqttEventGroup);

    char *Payload = (char *) malloc(LudoPayloadSize * sizeof(char));
    if (Payload == NULL) {
        // Fehlerbehandlung für fehlgeschlagene Speicherzuweisung
        ESP_LOGE(TAG, "Failed to allocate memory for Payload");
        return;
    }
    snprintf(Payload, LudoPayloadSize, "{\"macAddrHex\":\"%s\"}", LanPrintMac());

    // Null-Terminierung sicherstellen
    Payload[LudoPayloadSize - 1] = '\0';

    //Set the Topic to '/ThingName/Settings/pub'
    char pcTopic[LudoTopicSize];
    snprintf(pcTopic,LudoTopicSize, "device/settings/%s/request", LanPrintMac());


    //Frage nach den settings!
    ludoPublishToTopic(pcTopic, Payload);

    free(Payload);
    while( 1 )
    {
        prvWaitForEvent( SettingsMqttEventGroup, MQTT_INCOMING_PUBLISH_RECEIVED_BIT );
        ESP_LOGI(TAG, "Settings received: %s", SettingsIncomingPublishCallbackContext.pcIncomingPublish );

        //todo subscribe all channel and act after it
        JsonParse(SettingsIncomingPublishCallbackContext.pcIncomingPublish, "settings");
        //Lösche LED Task und starte NFC Scanner
        if(!NFCStarted())
        {
            StartNFC();
            RgbLedAWSConnected();
        }
        

        vTaskDelay( pdMS_TO_TICKS( subpubunsubconfigDELAY_BETWEEN_SUB_PUB_UNSUB_LOOPS_MS ) );
    }

    vEventGroupDelete( SettingsMqttEventGroup );
    vTaskDelete( NULL );
}

/* Public function definitions ************************************************/

void vStartSubscribePublishUnsubscribeDemo( void )
{   
    ESP_LOGI(TAG, "Starting SubscribeTask");
    xMessageIdSemaphore = xSemaphoreCreateMutex();
    xNetworkEventGroup = xEventGroupCreate();
    xCoreMqttAgentManagerRegisterHandler( prvCoreMqttAgentEventHandler );

    /* Initialize the coreMQTT-Agent event group. */
    xEventGroupSetBits( xNetworkEventGroup,
                        CORE_MQTT_AGENT_OTA_NOT_IN_PROGRESS_BIT );
    xTaskCreate(ludoSettingsTask, "ludoSettingsTask", SettingsTaskStackSize ,NULL, SettingsTaskPriority,NULL);
}
