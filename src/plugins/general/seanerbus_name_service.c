plug_identity()
{
    *tag = "seanerbus_name_service";
    *tags_deps = "!seanerbus";
}

plug_init_post(_seanerbus_name_service)
{
    metrics_call_t info = {0};
    info.input.command   = Info;
    info.input.device    = All;
    info.input.metric    = All;
    info.input.device_no = All;
    plug_message_send("received_request", (void *) &info);
}