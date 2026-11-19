local CreateThread, ResumeThread, ThreadStatus, YieldThread = coroutine.create, coroutine.resume, coroutine.status, coroutine.yield

local function MakeTasks()
    local Tasks = {}
    local Context = {}

    function Context:Spawn(Function)
        Tasks[#Tasks + 1] = { Thread = CreateThread(Function), Delay = 0 }
    end

    function Context:Wait(Seconds)
        assert(type(Seconds) == "number" and Seconds >= 0 and Seconds < math.huge, "invalid coroutine delay")
        return YieldThread(Seconds)
    end

    function Context:WaitUntil(Predicate)
        assert(type(Predicate) == "function", "coroutine predicate must be a function")
        return YieldThread(Predicate)
    end

    function Context:Advance(DeltaTime)
        local Current = Tasks
        Tasks = {}
        for Index = 1, #Current do
            local Task = Current[Index]
            Task.Delay = Task.Delay - DeltaTime
            if Task.Delay <= 0 and (Task.Predicate == nil or Task.Predicate()) then
                local Succeeded, Yielded = ResumeThread(Task.Thread)
                if not Succeeded then
                    error(Yielded, 0)
                end
                if ThreadStatus(Task.Thread) ~= "dead" then
                    Task.Delay = type(Yielded) == "number" and Yielded or 0
                    Task.Predicate = type(Yielded) == "function" and Yielded or nil
                end
            end
            if ThreadStatus(Task.Thread) ~= "dead" then
                Tasks[#Tasks + 1] = Task
            end
        end
    end

    return Context, Context.Advance
end

local function CaptureCallbacks(Environment, EventNames)
    local Callbacks = {}
    for _, Name in ipairs(EventNames) do
        local Callback = rawget(Environment, Name)
        if Callback ~= nil then
            assert(type(Callback) == "function", Name .. " must be a function")
            Callbacks[Name] = Callback
        end
    end
    return Callbacks
end

return { MakeTasks = MakeTasks, CaptureCallbacks = CaptureCallbacks }
