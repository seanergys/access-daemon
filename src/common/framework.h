#ifndef FRAMEWORK_H
#define FRAMEWORK_H

#include <common/system/plugin_manager.h>
                                                        // Relevant arguments
#define plug_identity          declr_up_get_tag         // (cchar **tag, cchar **tags_deps)
#define plug_action_init       declr_up_action_init     // (void **data_alloc, void *data)
#define plug_action_periodic   declr_up_action_periodic // (void *data)
#define plug_message_receive   declr_up_message_receive // (void *data)
#define plug_message_send      plugin_manager_message_send // (void *data)
#define plug_message_send_each plugin_manager_message_send_each // (void **data)

#endif // FRAMEWORK_H