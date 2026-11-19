local State = {
    mAlternatePunch = false,
    mAlternateHit = false,
}

local Settings = {
    MoveSpeed = 3.5,
    WalkSpeed = 1.5,
    SprintSpeed = 6.0,
    Acceleration = 24.0,
    JumpSpeed = 6.0,
    Gravity = 15.0,
    TurnSpeed = 10.0,
    StepHeight = 0.35,
    CameraDistance = 4.0,
    LookSensitivity = 0.003,
}

local Clip = {
    Hit_Chest = 7,
    Hit_Head = 8,
    Interact = 12,
    Pistol_Reload = 22,
    Pistol_Shoot = 23,
    Punch_Cross = 24,
    Punch_Jab = 25,
    Roll = 27,
    Spell_Simple_Shoot = 35,
    Sword_Attack = 39,
}

local UnitX = Vector.new(1, 0, 0)
local UnitY = Vector.new(0, 1, 0)
local UnitZ = Vector.new(0, 0, 1)
local Back = Vector.new(-1, 0, 0)
local Down = Vector.new(0, 0, -1)
local WallProbeHeights = { 0.40, 1.60 }

local function Clamp(Value, Minimum, Maximum)
    return math.max(Minimum, math.min(Value, Maximum))
end

local function WrapAngle(Angle)
    return (Angle + math.pi) % (2 * math.pi) - math.pi
end

local function Axis(Positive, Negative)
    return (Input.IsDown(Positive) and 1 or 0) - (Input.IsDown(Negative) and 1 or 0)
end

local function UpdateAnimation()
    State.mMesh:SetAnimGraphFloat("Speed", State.mSpeed)
    State.mMesh:SetAnimGraphFloat("VerticalSpeed", State.mVerticalSpeed)
    State.mMesh:SetAnimGraphFloat("AimPitch", Clamp(State.mPitch / 0.58, -1, 1))
    State.mMesh:SetAnimGraphFloat("AimWeight", Clamp(1 - State.mSpeed / 5, 0.45, 1))
    State.mMesh:SetAnimGraphBool("IsInAir", not State.mGrounded)
    State.mMesh:SetAnimGraphBool("IsCrouching", State.mCrouch)
    State.mMesh:SetAnimGraphBool("IsSwimming", State.mSwim)
    State.mMesh:SetAnimGraphBool("IsAiming", State.mAim)
    State.mMesh:SetAnimGraphBool("WantsAction", State.mAction)
    State.mMesh:SetAnimGraphBool("WantsHit", State.mHit)
    State.mMesh:SetAnimGraphBool("IsDancing", State.mDance)
    State.mMesh:SetAnimGraphBool("IsDead", State.mDead)
end

local function ResetPlayer()
    Actor:SetActorTransform(State.mSpawn)
    State.mVelocity = Vector.new(0, 0, 0)
    State.mVerticalSpeed = 0
    State.mFacing = 0
    State.mYaw = 0
    State.mPitch = -0.22
    State.mCameraArm = 0
    State.mSpeed = 0
    State.mBusyAge = 0
    State.mGrounded = true
    State.mCrouch = false
    State.mSwim = false
    State.mAim = false
    State.mDance = false
    State.mDead = false
    State.mAction = false
    State.mHit = false
    State.mBusy = ""
    State.mActionName = ""

    UpdateAnimation()
end

local function StartAction(Name)
    State.mActionName = Name
    State.mMesh:SetAnimGraphFloat("ActionClip", Clip[Name])
    State.mMesh:RestartAnimGraphPlayer("Action Player")
    State.mAction = true
    State.mHit = false
    State.mDance = false
    State.mBusy = "Action"
    State.mBusyAge = 0
end

local function StartHit()
    State.mMesh:SetAnimGraphFloat("HitClip", Clip[State.mAlternateHit and "Hit_Head" or "Hit_Chest"])
    State.mMesh:RestartAnimGraphPlayer("Hit Player")
    State.mAlternateHit = not State.mAlternateHit
    State.mHit = true
    State.mAction = false
    State.mDance = false
    State.mBusy = "Hit"
    State.mBusyAge = 0
end

local function UpdateControls()
    if Input.WasPressed(Key.Delete) then
        State.mDead = true
        State.mDance = false
        State.mBusy = ""
        State.mAction = false
        State.mHit = false
    end

    if State.mDead then
        return
    end

    if Input.WasPressed(Key.C) and State.mGrounded then
        State.mCrouch = not State.mCrouch
        State.mSwim = false
    end

    if Input.WasPressed(Key.V) and State.mGrounded then
        State.mSwim = not State.mSwim
        State.mCrouch = false
    end

    State.mAim = not State.mSwim and not State.mCrouch and Input.IsDown(Key.MouseRight)

    if Input.WasPressed(Key.H) and State.mGrounded then
        StartHit()
    elseif State.mBusy == "" and State.mGrounded and not State.mSwim then
        if Input.WasPressed(Key.F) then
            State.mDance = not State.mDance
        elseif Input.WasPressed(Key.Q) then
            StartAction("Roll")
        elseif Input.WasPressed(Key.R) then
            StartAction("Pistol_Reload")
        elseif Input.WasPressed(Key.E) then
            StartAction("Interact")
        elseif Input.WasPressed(Key.X) then
            StartAction("Sword_Attack")
        elseif Input.WasPressed(Key.K) then
            StartAction("Spell_Simple_Shoot")
        elseif Input.WasPressed(Key.MouseLeft) then
            StartAction(State.mAim and "Pistol_Shoot" or (State.mAlternatePunch and "Punch_Cross" or "Punch_Jab"))
            State.mAlternatePunch = not State.mAlternatePunch
        end
    end

    if Input.WasPressed(Key.Space) and State.mGrounded and State.mBusy == "" and not State.mSwim and not State.mDance then
        State.mGrounded = false
        State.mVerticalSpeed = Settings.JumpSpeed
    end
end

local function MoveAlong(Position, Direction, Distance)
    if math.abs(Distance) < 0.00001 then
        return Position
    end

    local TraceDirection = Direction * (Distance > 0 and 1 or -1)
    local Allowed = math.abs(Distance)
    for _, Height in ipairs(WallProbeHeights) do
        local Wall = World.Raycast(Position + UnitZ * Height, TraceDirection, Actor)
        if Wall.IsHit then
            Allowed = math.min(Allowed, math.max(0, Wall.Distance - 0.30))
        end
    end

    return Position + TraceDirection * Allowed
end

local function MovePlayer(Step)
    local Forward = Vector.new(math.cos(State.mYaw), math.sin(State.mYaw), 0)
    local Right = Vector.new(-Forward.y, Forward.x, 0)
    local Direction = Forward * Axis(Key.W, Key.S) + Right * Axis(Key.D, Key.A)
    if Direction:LengthSquared() > 1 then
        Direction = Direction:Normalized()
    end

    local Sprint = Input.IsDown(Key.Shift) or Input.IsDown(Key.LeftShift) or Input.IsDown(Key.RightShift)
    local TargetSpeed = State.mSwim and 2.2 or ((State.mCrouch or State.mAim or Input.IsDown(Key.Z)) and Settings.WalkSpeed or (Sprint and Settings.SprintSpeed or Settings.MoveSpeed))
    local Rolling = State.mBusy == "Action" and State.mActionName == "Roll"
    local Blocked = State.mDead or State.mDance or (State.mBusy ~= "" and not Rolling)
    local Desired = Direction * TargetSpeed
    if Blocked then
        Desired = Vector.new(0, 0, 0)
    elseif Rolling then
        Desired = Vector.new(math.cos(State.mFacing), math.sin(State.mFacing), 0) * 4.4
    end

    local Difference = Desired - State.mVelocity
    local Change = Settings.Acceleration * (State.mGrounded and 1 or 0.35) * Step
    State.mVelocity = State.mVelocity + (Difference:LengthSquared() > Change * Change and Difference:Normalized() * Change or Difference)

    if State.mAim or (not Blocked and not Rolling and Direction:LengthSquared() > 0.0001) then
        local Target = State.mAim and State.mYaw or math.atan2(Direction.y, Direction.x)
        local Turn = WrapAngle(Target - State.mFacing)
        local MaxTurn = Settings.TurnSpeed * Step
        State.mFacing = WrapAngle(State.mFacing + Clamp(Turn, -MaxTurn, MaxTurn))
    end

    local Player = Actor:GetActorTransform()
    local Previous = Player.Location
    local Position = MoveAlong(Previous, UnitX, State.mVelocity.x * Step)
    Position = MoveAlong(Position, UnitY, State.mVelocity.y * Step)
    local Horizontal = Vector.new(Position.x - Previous.x, Position.y - Previous.y, 0)
    State.mSpeed = Step > 0 and Horizontal:Length() / Step or 0
    State.mVerticalSpeed = State.mVerticalSpeed - Settings.Gravity * Step
    local VerticalDistance = State.mVerticalSpeed * Step

    if VerticalDistance > 0 then
        local Ceiling = World.Raycast(Position + UnitZ * 1.75, UnitZ, Actor)
        if Ceiling.IsHit and Ceiling.Distance < VerticalDistance + 0.05 then
            VerticalDistance = math.max(0, Ceiling.Distance - 0.05)
            State.mVerticalSpeed = 0
        end
    end

    Position = Position + UnitZ * VerticalDistance
    local Probe = Vector.new(Position.x, Position.y, math.max(Previous.z, Position.z) + Settings.StepHeight)
    local Ground = World.Raycast(Probe, Down, Actor)
    local GroundHeight = Ground.Position.z + 0.025
    local SnapDistance = State.mGrounded and Settings.StepHeight or 0
    local Reachable = Ground.IsHit and GroundHeight <= Previous.z + SnapDistance + 0.01
    local Contact = Position.z <= GroundHeight or (State.mGrounded and Position.z - GroundHeight <= Settings.StepHeight)
    State.mGrounded = State.mVerticalSpeed <= 0 and Reachable and Contact

    if State.mGrounded then
        Position.z = GroundHeight
        State.mVerticalSpeed = 0
    end

    if Position.z < -10 then
        ResetPlayer()
        return
    end

    Player.Location = Position
    Player.Rotation = Quat.new(UnitZ, State.mFacing) * State.mSpawn.Rotation
    Actor:SetActorTransform(Player)
end

local function UpdateCamera(Step)
    local Rotation = (Quat.new(UnitZ, State.mYaw) * Quat.new(UnitY, -State.mPitch)):Normalized()
    local Target = Actor.Location + UnitZ * (State.mCrouch and 1.12 or 1.4)
    local Direction = Rotation:RotateVector(Back)
    local Obstacle = World.Raycast(Target, Direction, Actor)
    local Distance = Settings.CameraDistance * (State.mAim and 0.75 or 1)
    local Limit = Obstacle.IsHit and Clamp(Obstacle.Distance - 0.20, 0.20, Distance) or Distance
    State.mCameraArm = State.mCameraArm <= 0 and Limit or math.min(Limit, State.mCameraArm + Step * 8)

    local Camera = State.mCamera:GetActorTransform()
    Camera.Location = Target + Direction * State.mCameraArm
    Camera.Rotation = Rotation
    State.mCamera:SetActorTransform(Camera)
end

function BeginPlay()
    State.mMesh = assert(Actor:GetComponentByClass("USkeletalMeshComponent"), "The third person character needs a skeletal mesh.")
    State.mCamera = assert(World.FindActor("Follow Camera"), "The follow camera is missing.")
    assert(World.SetActiveCamera(State.mCamera), "The follow camera needs a camera component.")
    State.mSpawn = Actor:GetActorTransform()

    ResetPlayer()
    UpdateCamera(0)
end

function Tick(DeltaTime)
    local Step = Clamp(DeltaTime, 0, 0.05)
    local MouseX, MouseY = Input.GetMouseDelta()
    State.mYaw = WrapAngle(State.mYaw + MouseX * Settings.LookSensitivity)
    State.mPitch = Clamp(State.mPitch - MouseY * Settings.LookSensitivity, -1.15, 0.9)
    State.mBusyAge = State.mBusyAge + Step

    if Input.WasPressed(Key.Backspace) then
        ResetPlayer()
    end

    UpdateControls()

    if State.mBusyAge > 0.12 and State.mBusy == "Action" and State.mMesh:IsAnimGraphPlayerFinished("Action Player") then
        State.mBusy = ""
        State.mAction = false
    end

    if State.mBusyAge > 0.12 and State.mBusy == "Hit" and State.mMesh:IsAnimGraphPlayerFinished("Hit Player") then
        State.mBusy = ""
        State.mHit = false
    end

    MovePlayer(Step)
    UpdateAnimation()
    UpdateCamera(Step)
end
