local Script = {}
Script.__index = Script

function Script:BeginPlay()
    print("[BeginPlay] " .. self.Actor.UUID)
end

function Script:Tick(DeltaTime)
end

function Script:OnOverlap(Other)
end

function Script:OnEndOverlap(Other)
end

function Script:OnHit(Other, Hit)
end

function Script:EndPlay()
    print("[EndPlay] " .. self.Actor.UUID)
end

return function(Actor, Tasks)
    return setmetatable({ Actor = Actor, Tasks = Tasks }, Script)
end
