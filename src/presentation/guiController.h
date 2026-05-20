// === src/presentation/guiController.h ===

#ifndef GUI_CONTROLLER_H
#define GUI_CONTROLLER_H

typedef struct GuiController GuiController;

struct SystemController;

typedef int
(*CommandCallback)
(
    struct SystemController* controller,
    int commandId,
    const void* args
);

GuiController* guiControllerCreate(void);

void guiControllerDestroy(GuiController* controller);

int guiControllerRegisterCommand(GuiController* controller, int commandId, CommandCallback callback);

int guiControllerExecuteCommand(GuiController* controller, int commandId, const void* args);

int guiControllerHandleInput(GuiController* controller);

#endif
