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

---@enum MacawDepthOfFieldApertureShape
DepthOfFieldApertureShape = {
    Circle = 0,
    Pentagon = 1,
    Hexagon = 2,
}

---@class MacawDepthOfFieldSettings
---@field Enabled boolean
---@field FocalDistance number 미터
---@field FocusWidth number 미터
---@field TransitionDistance number 미터
---@field MaxBlurRadius number 전체 해상도 픽셀
---@field FStop number
---@field ApertureShape MacawDepthOfFieldApertureShape
DepthOfFieldSettings = {}

---@return MacawDepthOfFieldSettings
function DepthOfFieldSettings.new() end

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

---@class MacawComponent
---@field Name string
---@field Owner MacawActor
Component = {}

---@alias MacawSkeletalMeshComponent MacawComponent
SkeletalMeshComponent = Component

---@alias MacawCameraComponent MacawComponent
CameraComponent = Component

---@return boolean
function Component:IsValid() end

---@param Name string
---@param Value number
---@return boolean
function Component:SetAnimGraphFloat(Name, Value) end

---@param Name string
---@param Value boolean
---@return boolean
function Component:SetAnimGraphBool(Name, Value) end

---@param Name string
---@return boolean
function Component:RestartAnimGraphPlayer(Name) end

---@param Name string
---@return boolean
function Component:IsAnimGraphPlayerFinished(Name) end

--- 설정의 사본을 반환합니다. 카메라 컴포넌트가 아니면 nil을 반환합니다.
---@return MacawDepthOfFieldSettings?
function Component:GetDepthOfFieldSettings() end

---@param Value MacawDepthOfFieldSettings
---@return boolean
function Component:SetDepthOfFieldSettings(Value) end

---@param Distance number 미터
---@return boolean
function Component:SetDepthOfFieldFocalDistance(Distance) end

---@param Torque number N·m
---@param WheelIndex integer 0부터 시작하는 바퀴 인덱스
---@return boolean
function Component:SetDriveTorque(Torque, WheelIndex) end

---@param Torque number N·m
---@param WheelIndex integer 0부터 시작하는 바퀴 인덱스
---@return boolean
function Component:SetBrakeTorque(Torque, WheelIndex) end

---@param AngleDegrees number 도 단위 조향각
---@param WheelIndex integer 0부터 시작하는 바퀴 인덱스
---@return boolean
function Component:SetSteerAngle(AngleDegrees, WheelIndex) end

---@param Throttle number -1부터 1까지, 음수는 후진 요청
---@param Brake number 0부터 1까지
---@param Steering number -1부터 1까지
---@return boolean
function Component:SetDrive4WInput(Throttle, Brake, Steering) end

---@return boolean
function Component:ResetDrive4W() end

---@return integer Gear 후진 -1, 중립 0, 전진 1부터
---@return number ForwardSpeed m/s
function Component:GetDrive4WState() end

---@param Text string 월드 빌보드에 표시할 문자열
---@return boolean
function Component:SetText(Text) end

--- 현재 바디의 형상을 차량 서스펜션이 감지할 주행면으로 지정합니다.
---@param Drivable boolean
---@return boolean
function Component:SetVehicleDrivable(Drivable) end

---@param Linear MacawVector m/s
---@param Angular MacawVector rad/s
---@return boolean
function Component:SetPhysicsVelocity(Linear, Angular) end

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

--- 지정한 클래스 또는 그 파생 클래스에 해당하는 첫 번째 컴포넌트를 반환합니다.
--- 일치하는 컴포넌트가 없으면 nil을 반환합니다. 등록되지 않았거나 컴포넌트가 아닌 클래스 이름을 넘기면 오류가 발생합니다.
---@param ClassName string
---@return MacawComponent?
function Actor:GetComponentByClass(ClassName) end

---@return MacawComponent?
function Actor:GetSkeletalMeshComponent() end

---@return MacawCameraComponent?
function Actor:GetCameraComponent() end

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
