//
// Created by AICDG on 2020/11/8.
//

#include "Common_3/OS/Interfaces/IApp.h"

class MyApplication : public IApp
{
public:
    bool Init() override { return true; }
    void Exit() override {}

    bool Load() override { return true; }
    void Unload() override {}

    void Update(float deltaTime) override {}
    void Draw() override {}

    const char* GetName() override { return "window"; }
};

//DEFINE_APPLICATION_MAIN(MyApplication);

extern int WindowsMain(int argc, char** argv, IApp* app);

int main(int argc, char** argv)
{
    MyApplication app;
    return WindowsMain(argc, argv, &app);
}
