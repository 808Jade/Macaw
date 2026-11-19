---@meta

---@class MacawVector
---@field x number
---@field y number
---@field z number
---@operator add(MacawVector): MacawVector
---@operator sub(MacawVector): MacawVector
---@operator mul(number): MacawVector
local VectorValue = {}

---@return number
function VectorValue:Length() end

---@return number
function VectorValue:LengthSquared() end

---@return MacawVector
function VectorValue:Normalized() end

Vector = {}

---@overload fun(): MacawVector
---@param X number
---@param Y number
---@param Z number
---@return MacawVector
function Vector.new(X, Y, Z) end

---@class MacawRotator
---@field Pitch number
---@field Yaw number
---@field Roll number

Rotator = {}

---@overload fun(): MacawRotator
---@param Pitch number
---@param Yaw number
---@param Roll number
---@return MacawRotator
function Rotator.new(Pitch, Yaw, Roll) end

---@class MacawQuat
---@operator mul(MacawQuat): MacawQuat
local QuatValue = {}

---@return MacawQuat
function QuatValue:Normalized() end

---@param Value MacawVector
---@return MacawVector
function QuatValue:RotateVector(Value) end

Quat = {}

---@overload fun(): MacawQuat
---@param Axis MacawVector
---@param Angle number
---@return MacawQuat
function Quat.new(Axis, Angle) end

---@class MacawTransform
---@field Location MacawVector
---@field Rotation MacawQuat
---@field Scale MacawVector
local TransformValue = {}

---@return MacawRotator
function TransformValue:GetRotator() end

---@param Value MacawRotator
function TransformValue:SetRotator(Value) end

Transform = {}

---@return MacawTransform
function Transform.new() end

---@class MacawHitResult
---@field BlockingHit boolean
---@field ImpactPoint MacawVector
---@field Normal MacawVector
---@field Time number
HitResult = {}

---@class MacawRaycastHit
---@field IsHit boolean
---@field Position MacawVector
---@field Distance number
RaycastHit = {}

---@class MacawSkeletalMeshComponent
SkeletalMeshComponent = {}

---@param Name string
---@param Value number
---@return boolean
function SkeletalMeshComponent:SetAnimGraphFloat(Name, Value) end

---@param Name string
---@param Value boolean
---@return boolean
function SkeletalMeshComponent:SetAnimGraphBool(Name, Value) end

---@param Name string
---@return boolean
function SkeletalMeshComponent:RestartAnimGraphPlayer(Name) end

---@param Name string
---@return boolean
function SkeletalMeshComponent:IsAnimGraphPlayerFinished(Name) end

---@class MacawActor
---@field UUID string
---@field Location MacawVector
---@field Velocity MacawVector
Actor = {}

---@return boolean
function Actor:IsValid() end

---@return string
function Actor:GetName() end

---@return MacawTransform
function Actor:GetActorTransform() end

---@param Value MacawTransform
---@return boolean
function Actor:SetActorTransform(Value) end

---@return MacawSkeletalMeshComponent?
function Actor:GetSkeletalMeshComponent() end

function Actor:PrintLocation() end

---@param Delta MacawVector
---@return MacawHitResult
function Actor:MoveSwept(Delta) end

---@class MacawTasks
Tasks = {}

---@param Function fun()
function Tasks:Spawn(Function) end

---@param Seconds number
function Tasks:Wait(Seconds) end

---@param Predicate fun(): boolean
function Tasks:WaitUntil(Predicate) end

World = {}

---@param Name string
---@return MacawActor?
function World.FindActor(Name) end

---@param Origin MacawVector
---@param Direction MacawVector
---@param Ignored? MacawActor
---@return MacawRaycastHit
function World.Raycast(Origin, Direction, Ignored) end

---@param Value MacawActor
---@return boolean
function World.SetActiveCamera(Value) end

---@class MacawKeys
---@field W integer
---@field A integer
---@field S integer
---@field D integer
---@field C integer
---@field V integer
---@field F integer
---@field Q integer
---@field R integer
---@field E integer
---@field X integer
---@field K integer
---@field H integer
---@field Z integer
---@field Space integer
---@field Backspace integer
---@field Delete integer
---@field Shift integer
---@field LeftShift integer
---@field RightShift integer
---@field MouseLeft integer
---@field MouseRight integer
Key = {}

Input = {}

---@param KeyCode integer
---@return boolean
function Input.IsDown(KeyCode) end

---@param KeyCode integer
---@return boolean
function Input.WasPressed(KeyCode) end

---@return number X
---@return number Y
function Input.GetMouseDelta() end
