#ifndef dev_usb_gadget_setup_result_h
#define dev_usb_gadget_setup_result_h

namespace usb::gadget {

/*
 * The result of a USB setup request.
 */
enum class setup_result {
	/* Setup request failed, endpoint will be stalled. */
	error,

	/* Setup request requires a data phase & status phase.
	 *
	 * Device->Host
	 *   The data in the transaction buffer will be transferred, followed
	 *   by a status transaction on success.
	 *
	 * Host->Device
	 *   The USB device controller framework will provide a buffer to
	 *   receive the setup data and peform a data transaction. This will be
	 *   followed by a status transaction on success.
	 */
	data,

	/* Setup request completed & requires a status phase. */
	status,

	/* Setup request is complete, no further transactions performed. */
	complete,
};

}

#endif
