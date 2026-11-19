local RotatorScript = {}
RotatorScript.__index = RotatorScript

function RotatorScript:Tick(DeltaTime)
    local Transform = self.Actor:GetActorTransform()
    local Rotation = Transform:GetRotator()
    Rotation.Yaw = Rotation.Yaw + self.Speed * DeltaTime
    Transform:SetRotator(Rotation)
    self.Actor:SetActorTransform(Transform)
end

return function(Actor)
    return setmetatable({ Actor = Actor, Speed = 90 }, RotatorScript)
end
