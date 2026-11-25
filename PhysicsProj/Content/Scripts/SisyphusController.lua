local Settings = {
    WalkSpeed = 4.4,
    PushWalkSpeed = 2.8,
    Acceleration = 24.0,
    Gravity = 17.0,
    JumpSpeed = 5.2,
    FloorSnapDistance = 0.24,
    WalkableFloorZ = 0.7,
    PushRange = 1.40,
    PushSpeed = 3.3,
    PushGain = 430.0,
    PushSteeringGain = 470.0,
    MaxPushForce = 950.0,
    SummitRadius = 1.6,
    FallHeight = -8.0,
    CameraDistance = 5.4,
    LookSensitivity = 0.003,
}

local State = {}
local Zero = Vector.new()
local UnitX = Vector.new(1, 0, 0)
local UnitY = Vector.new(0, 1, 0)
local Up = Vector.new(0, 0, 1)

local function Clamp(Value, Minimum, Maximum)
    return math.max(Minimum, math.min(Value, Maximum))
end

local function WrapAngle(Angle)
    return (Angle + math.pi) % (2 * math.pi) - math.pi
end

local function Horizontal(Value)
    return Vector.new(Value.x, Value.y, 0)
end

local function Dot(A, B)
    return A.x * B.x + A.y * B.y + A.z * B.z
end

local function Axis(Positive, Negative)
    return (Input.IsDown(Positive) and 1 or 0) - (Input.IsDown(Negative) and 1 or 0)
end

local function UpdateCamera(Step)
    local Rotation = (Quat.new(Up, State.mYaw) * Quat.new(UnitY, -State.mPitch)):Normalized()
    local Target = Actor:GetActorLocation() + Up * 0.78
    local Back = Rotation:RotateVector(UnitX) * -1
    local Obstacle = World.Raycast(Target, Back, Actor)
    local Limit = Obstacle.IsHit and Clamp(Obstacle.Distance - 0.18, 0.22, Settings.CameraDistance) or Settings.CameraDistance
    State.mCameraArm = math.min(Limit, State.mCameraArm + Step * 9)

    local Camera = State.mCamera:GetActorTransform()
    Camera.Location = Target + Back * State.mCameraArm
    Camera.Rotation = Rotation
    State.mCamera:SetActorTransform(Camera)
end

local function UpdateStatus()
    local Height = Clamp(State.mStone:GetActorLocation().z - State.mStoneSpawn.Location.z, 0, State.mSummit.z - State.mStoneSpawn.Location.z)
    local Time = math.floor(State.mElapsed)
    State.mProgress:SetText(string.format("%04.1f / %.0f m    |    %d:%02d", Height, State.mSummit.z - State.mStoneSpawn.Location.z, math.floor(Time / 60), Time % 60))

    if State.mFinished then
        State.mPrompt:SetText("SUMMIT REACHED   /   R TO TRY AGAIN")
    elseif State.mRetryNotice > 0 then
        State.mPrompt:SetText("BACK AT THE FOOT OF THE HILL")
    else
        State.mPrompt:SetText("WASD  MOVE     MOUSE  LOOK     E + MOVE  PUSH     SPACE  JUMP     R  RESTART")
    end
end

local function ResetRun(Fell)
    Actor:SetActorTransform(State.mPlayerSpawn)
    State.mStone:SetActorTransform(State.mStoneSpawn)
    assert(State.mStoneBody:SetPhysicsVelocity(Zero, Zero), "The boulder needs a simulated sphere body.")

    State.mVelocity = Zero
    State.mStoneVelocity = Zero
    State.mLastStonePosition = State.mStoneSpawn.Location
    State.mVerticalSpeed = 0
    State.mGrounded = true
    State.mYaw = 0
    State.mPitch = -0.19
    State.mFacing = 0
    State.mCameraArm = Settings.CameraDistance
    State.mElapsed = 0
    State.mStarted = false
    State.mFinished = false
    State.mRetryNotice = Fell and 2.4 or 0
    State.mMesh:SetAnimGraphFloat("Speed", 0)
    State.mMesh:SetAnimGraphFloat("VerticalSpeed", 0)
    State.mMesh:SetAnimGraphBool("IsInAir", false)

    UpdateCamera(0)
    UpdateStatus()
end

local function SweepMove(Delta)
    return select(2, Actor:AddActorWorldOffset(Delta, true))
end

local function IsWalkable(Hit)
    return Hit.BlockingHit and Hit.Normal.z >= Settings.WalkableFloorZ
end

local function MoveHorizontal(Delta)
    for Attempt = 1, 3 do
        if Delta:LengthSquared() < 0.00000001 then
            return
        end

        local Hit = SweepMove(Delta)
        if not Hit.BlockingHit then
            return
        end

        local Normal = Hit.Normal
        if not IsWalkable(Hit) then
            Normal = Horizontal(Normal):Normalized()
        end

        local Remaining = Delta * (1 - Hit.Time)
        local IntoSurface = Dot(Remaining, Normal)
        if IntoSurface >= -0.0001 then
            return
        end

        Delta = Remaining - Normal * IntoSurface
    end
end

local function MoveVertical(Distance)
    local Hit = SweepMove(Up * Distance)
    if Distance > 0 then
        if Hit.BlockingHit then
            State.mVerticalSpeed = 0
        end

        State.mGrounded = false
        return
    end

    if not Hit.BlockingHit and State.mGrounded then
        local Start = Actor:GetActorLocation()
        Hit = SweepMove(Up * -Settings.FloorSnapDistance)
        if not IsWalkable(Hit) then
            Actor:SetActorLocation(Start)
        end
    end

    State.mGrounded = IsWalkable(Hit)
    if State.mGrounded or Hit.BlockingHit then
        State.mVerticalSpeed = 0
    end
end

local function MovePlayer(Direction, Pushing, Step)
    local Desired = Direction * (Pushing and Settings.PushWalkSpeed or Settings.WalkSpeed)
    local Difference = Desired - State.mVelocity
    local Change = Settings.Acceleration * (State.mGrounded and 1 or 0.4) * Step
    State.mVelocity = State.mVelocity + (Difference:LengthSquared() > Change * Change and Difference:Normalized() * Change or Difference)

    if Input.WasPressed(Key.Space) and State.mGrounded and not Pushing then
        State.mVerticalSpeed = Settings.JumpSpeed
        State.mGrounded = false
    end

    local Previous = Actor:GetActorLocation()
    MoveHorizontal(State.mVelocity * Step)
    State.mVerticalSpeed = State.mVerticalSpeed - Settings.Gravity * Step
    MoveVertical(State.mVerticalSpeed * Step)

    if Direction:LengthSquared() > 0.0001 then
        local Target = math.atan2(Direction.y, Direction.x)
        local Turn = WrapAngle(Target - State.mFacing)
        State.mFacing = WrapAngle(State.mFacing + Clamp(Turn, -12 * Step, 12 * Step))
    end

    local Player = Actor:GetActorTransform()
    Player.Rotation = Quat.new(Up, State.mFacing) * State.mPlayerSpawn.Rotation
    Actor:SetActorTransform(Player)

    local Speed = Horizontal(Player.Location - Previous):Length() / Step
    State.mMesh:SetAnimGraphFloat("Speed", Speed)
    State.mMesh:SetAnimGraphFloat("VerticalSpeed", State.mVerticalSpeed)
    State.mMesh:SetAnimGraphBool("IsInAir", not State.mGrounded)
end

local function CanPush(Direction)
    if State.mFinished or not State.mGrounded or not Input.IsDown(Key.E) then
        return false
    end

    local Offset = State.mStone:GetActorLocation() - Actor:GetActorLocation()
    local Toward = Horizontal(Offset)
    local Distance = Toward:Length()
    if Distance < 0.1 or Distance > Settings.PushRange or math.abs(Offset.z) > 0.85 then
        return false
    end

    return Dot(Toward, Direction) >= 0.45 * Distance
end

local function UpdateRun(DeltaTime, Step)
    local StonePosition = State.mStone:GetActorLocation()
    if Actor:GetActorLocation().z < Settings.FallHeight or StonePosition.z < Settings.FallHeight then
        ResetRun(true)
        return false
    end

    local Sample = (StonePosition - State.mLastStonePosition) * (1 / DeltaTime)
    State.mStoneVelocity = State.mStoneVelocity + (Sample - State.mStoneVelocity) * math.min(12 * Step, 1)
    State.mLastStonePosition = StonePosition

    State.mRetryNotice = math.max(0, State.mRetryNotice - DeltaTime)
    if State.mStarted and not State.mFinished then
        State.mElapsed = State.mElapsed + DeltaTime
        local Offset = StonePosition - State.mSummit
        if math.abs(Offset.z) < 0.45 and Horizontal(Offset):LengthSquared() < Settings.SummitRadius * Settings.SummitRadius then
            State.mFinished = true
            State.mStoneBody:SetPhysicsVelocity(Zero, Zero)
        end
    end

    return true
end

function BeginPlay()
    State.mPlayerSpawn = Actor:GetActorTransform()
    State.mMesh = assert(Actor:GetSkeletalMeshComponent())
    State.mCamera = assert(World.FindActor("Ascent Camera"))
    State.mProgress = assert(State.mCamera:GetComponentByName("Progress"))
    State.mPrompt = assert(State.mCamera:GetComponentByName("Controls"))
    State.mStone = assert(World.FindActor("Boulder"))
    State.mStoneSpawn = State.mStone:GetActorTransform()
    State.mStoneBody = assert(State.mStone:GetComponentByClass("USphereComponent"))
    State.mSummit = assert(World.FindActor("Summit")):GetActorLocation()
    assert(World.SetActiveCamera(State.mCamera))

    ResetRun(false)
end

function Tick(DeltaTime)
    if Input.WasPressed(Key.R) then
        ResetRun(false)
        return
    end

    if DeltaTime <= 0 then
        return
    end

    local Step = math.min(DeltaTime, 0.05)
    if not UpdateRun(DeltaTime, Step) then
        return
    end

    local MouseX, MouseY = Input.GetMouseDelta()
    State.mYaw = WrapAngle(State.mYaw + MouseX * Settings.LookSensitivity)
    State.mPitch = Clamp(State.mPitch - MouseY * Settings.LookSensitivity, -1.14, 0.63)

    local Forward = Vector.new(math.cos(State.mYaw), math.sin(State.mYaw), 0)
    local Right = Vector.new(-Forward.y, Forward.x, 0)
    local Direction = Forward * Axis(Key.W, Key.S) + Right * Axis(Key.D, Key.A)
    if Direction:LengthSquared() > 1 then
        Direction = Direction:Normalized()
    end

    local Pushing = CanPush(Direction)
    if Pushing then
        local Speed = Dot(State.mStoneVelocity, Direction)
        local Force = Clamp((Settings.PushSpeed - Speed) * Settings.PushGain, 0, Settings.MaxPushForce)
        local SideVelocity = Horizontal(State.mStoneVelocity) - Direction * Speed
        local Push = Direction * Force - SideVelocity * Settings.PushSteeringGain
        if Push:LengthSquared() > 0.01 and State.mStoneBody:AddForce(Push) then
            State.mStarted = true
        end
    end

    MovePlayer(Direction, Pushing, Step)
    UpdateCamera(Step)
    UpdateStatus()
end
