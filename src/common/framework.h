#ifndef FRAMEWORK_H
#define FRAMEWORK_H

#include <common/system/plugin_manager.h>

#define plug_identity          declr_up_get_tag
#define plug_action_init       declr_up_action_init
#define plug_action_periodic   declr_up_action_periodic
#define plug_message_receive   declr_up_message_receive
#define plug_poll_attend       declr_up_poll_attend
#define plug_message_send      plugin_manager_message_send
#define plug_message_send_each plugin_manager_message_send_each

#endif // FRAMEWORK_H