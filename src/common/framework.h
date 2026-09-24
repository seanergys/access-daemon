#ifndef FRAMEWORK_H
#define FRAMEWORK_H

#include <common/system/plugin_manager.h>
                                                         // Relevant arguments
#define plug_identity          declr_up_get_tag          // (cchar **tag, cchar **tags_deps)
#define plug_init              declr_up_action_init      // (void **data_alloc, void *data)
#define plug_init_post         declr_up_action_init_post // (void *data)
#define plug_periodic          declr_up_action_periodic  // (void *data)
#define plug_exit              declr_up_action_close     //
#define plug_message_receive   declr_up_message_receive  // (void *data)
#define plug_poll_attend       declr_up_poll_attend      // (cchar *tag_fd, struct pollfd *pfd)

#define plug_message_send      plugin_manager_message_send // (void *data)
#define plug_poll_add          plugin_manager_poll_add   // (cchar *tag, cchar *tag_fd, int fd)

#endif // FRAMEWORK_H