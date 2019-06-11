#ifndef __MESSAGES_H
#define __MESSAGES_H

#define MSG_MAX_SIZE 256

struct message {
	// Message type. Must be > 0 
	// Usage:
	// - client id in user connection request
	// - addresser id when the message is sent to
	//    the another client
	int type;
	/* Data content */
	char content[MSG_MAX_SIZE];
};


#endif // __MESSAGES_H

