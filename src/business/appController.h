#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

typedef struct AppController AppController;

AppController* appControllerCreate(void);
int appControllerRun(AppController* controller);
void appControllerDestroy(AppController* controller);

#endif
