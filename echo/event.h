#ifndef _EVENT_H
#define _EVENT_H 1

void event_init(int);
void event_process();
void event_add_listenfd(int);

#endif /*_EVENT_H*/
