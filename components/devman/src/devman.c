#include "devman.h"
#include "astarte_handler.h"

void devman_init(void){
    astarte_device_handle_t astarte_device = astarteHandler_init();

    if(astarte_device){
        astarteHandler_start(astarte_device);
    }
}
