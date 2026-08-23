// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#pragma once

#include <tlRender/Timeline/Export.h>
#include <tlRender/IO/Read.h>
#include <tlRender/IO/Write.h>

#include <ftk/Core/ISystem.h>

namespace tl
{
    class Player;

    //! Timeline system.
    class TL_TIMELINE_API_TYPE System : public ftk::ISystem
    {
        FTK_NON_COPYABLE(System);

    protected:
        System(const std::shared_ptr<ftk::Context>&);

    public:
        TL_TIMELINE_API virtual ~System();

        //! Create a new system.
        TL_TIMELINE_API static std::shared_ptr<System> create(const std::shared_ptr<ftk::Context>&);

        TL_TIMELINE_API void tick() override;
        TL_TIMELINE_API std::chrono::milliseconds getTickTime() const override;

    private:
        void _addPlayer(const std::shared_ptr<Player>&);

        friend class Player;

        FTK_PRIVATE();
    };
}
