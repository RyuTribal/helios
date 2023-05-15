#pragma once

namespace hve
{
    struct HVEAnimComponent
    {
        float frameTime;
        int currentFrame;
        /* TODO: Will need to be expanded to refer
         * to some sprite tile, what type of tile
         * play next and so on
        */
        HVEAnimComponent(float fTime = 0.0f, int cFrame = 0)
            : frameTime(fTime), currentFrame(cFrame) {}
    };
}