local Controller = {}
Controller.__index = Controller

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

function Controller:BeginPlay()
    self.mMesh = assert(self.mActor:GetSkeletalMeshComponent(), "The third person character needs a skeletal mesh.")
    self.mCamera = assert(World.FindActor("Follow Camera"), "The follow camera is missing.")
    assert(World.SetActiveCamera(self.mCamera), "The follow camera needs a camera component.")
    self.mSpawn = self.mActor:GetActorTransform()

    self:ResetPlayer()
    self:UpdateCamera(0)
end

function Controller:Tick(DeltaTime)
    local Step = Clamp(DeltaTime, 0, 0.05)
    local MouseX, MouseY = Input.GetMouseDelta()
    self.mYaw = WrapAngle(self.mYaw + MouseX * Settings.LookSensitivity)
    self.mPitch = Clamp(self.mPitch - MouseY * Settings.LookSensitivity, -1.15, 0.9)
    self.mBusyAge = self.mBusyAge + Step

    if Input.WasPressed(Key.Backspace) then
        self:ResetPlayer()
    end

    self:UpdateControls()

    if self.mBusyAge > 0.12 and self.mBusy == "Action" and self.mMesh:IsAnimGraphPlayerFinished("Action Player") then
        self.mBusy = ""
        self.mAction = false
    end

    if self.mBusyAge > 0.12 and self.mBusy == "Hit" and self.mMesh:IsAnimGraphPlayerFinished("Hit Player") then
        self.mBusy = ""
        self.mHit = false
    end

    self:MovePlayer(Step)
    self:UpdateAnimation()
    self:UpdateCamera(Step)
end

function Controller:ResetPlayer()
    self.mActor:SetActorTransform(self.mSpawn)
    self.mVelocity = Vector.new(0, 0, 0)
    self.mVerticalSpeed = 0
    self.mFacing = 0
    self.mYaw = 0
    self.mPitch = -0.22
    self.mCameraArm = 0
    self.mSpeed = 0
    self.mBusyAge = 0
    self.mGrounded = true
    self.mCrouch = false
    self.mSwim = false
    self.mAim = false
    self.mDance = false
    self.mDead = false
    self.mAction = false
    self.mHit = false
    self.mBusy = ""
    self.mActionName = ""

    self:UpdateAnimation()
end

function Controller:UpdateControls()
    if Input.WasPressed(Key.Delete) then
        self.mDead = true
        self.mDance = false
        self.mBusy = ""
        self.mAction = false
        self.mHit = false
    end

    if self.mDead then
        return
    end

    if Input.WasPressed(Key.C) and self.mGrounded then
        self.mCrouch = not self.mCrouch
        self.mSwim = false
    end

    if Input.WasPressed(Key.V) and self.mGrounded then
        self.mSwim = not self.mSwim
        self.mCrouch = false
    end

    self.mAim = not self.mSwim and not self.mCrouch and Input.IsDown(Key.MouseRight)

    if Input.WasPressed(Key.H) and self.mGrounded then
        self:StartHit()
    elseif self.mBusy == "" and self.mGrounded and not self.mSwim then
        if Input.WasPressed(Key.F) then
            self.mDance = not self.mDance
        elseif Input.WasPressed(Key.Q) then
            self:StartAction("Roll")
        elseif Input.WasPressed(Key.R) then
            self:StartAction("Pistol_Reload")
        elseif Input.WasPressed(Key.E) then
            self:StartAction("Interact")
        elseif Input.WasPressed(Key.X) then
            self:StartAction("Sword_Attack")
        elseif Input.WasPressed(Key.K) then
            self:StartAction("Spell_Simple_Shoot")
        elseif Input.WasPressed(Key.MouseLeft) then
            self:StartAction(self.mAim and "Pistol_Shoot" or (self.mAlternatePunch and "Punch_Cross" or "Punch_Jab"))
            self.mAlternatePunch = not self.mAlternatePunch
        end
    end

    if Input.WasPressed(Key.Space) and self.mGrounded and self.mBusy == "" and not self.mSwim and not self.mDance then
        self.mGrounded = false
        self.mVerticalSpeed = Settings.JumpSpeed
    end
end

function Controller:StartAction(Name)
    self.mActionName = Name
    self.mMesh:SetAnimGraphFloat("ActionClip", Clip[Name])
    self.mMesh:RestartAnimGraphPlayer("Action Player")
    self.mAction = true
    self.mHit = false
    self.mDance = false
    self.mBusy = "Action"
    self.mBusyAge = 0
end

function Controller:StartHit()
    self.mMesh:SetAnimGraphFloat("HitClip", Clip[self.mAlternateHit and "Hit_Head" or "Hit_Chest"])
    self.mMesh:RestartAnimGraphPlayer("Hit Player")
    self.mAlternateHit = not self.mAlternateHit
    self.mHit = true
    self.mAction = false
    self.mDance = false
    self.mBusy = "Hit"
    self.mBusyAge = 0
end

function Controller:MovePlayer(Step)
    local Forward = Vector.new(math.cos(self.mYaw), math.sin(self.mYaw), 0)
    local Right = Vector.new(-Forward.y, Forward.x, 0)
    local Direction = Forward * Axis(Key.W, Key.S) + Right * Axis(Key.D, Key.A)
    if Direction:LengthSquared() > 1 then
        Direction = Direction:Normalized()
    end

    local Sprint = Input.IsDown(Key.Shift) or Input.IsDown(Key.LeftShift) or Input.IsDown(Key.RightShift)
    local TargetSpeed = self.mSwim and 2.2 or ((self.mCrouch or self.mAim or Input.IsDown(Key.Z)) and Settings.WalkSpeed or (Sprint and Settings.SprintSpeed or Settings.MoveSpeed))
    local Rolling = self.mBusy == "Action" and self.mActionName == "Roll"
    local Blocked = self.mDead or self.mDance or (self.mBusy ~= "" and not Rolling)
    local Desired = Direction * TargetSpeed
    if Blocked then
        Desired = Vector.new(0, 0, 0)
    elseif Rolling then
        Desired = Vector.new(math.cos(self.mFacing), math.sin(self.mFacing), 0) * 4.4
    end

    local Difference = Desired - self.mVelocity
    local Change = Settings.Acceleration * (self.mGrounded and 1 or 0.35) * Step
    self.mVelocity = self.mVelocity + (Difference:LengthSquared() > Change * Change and Difference:Normalized() * Change or Difference)

    if self.mAim or (not Blocked and not Rolling and Direction:LengthSquared() > 0.0001) then
        local Target = self.mAim and self.mYaw or math.atan2(Direction.y, Direction.x)
        local Turn = WrapAngle(Target - self.mFacing)
        local MaxTurn = Settings.TurnSpeed * Step
        self.mFacing = WrapAngle(self.mFacing + Clamp(Turn, -MaxTurn, MaxTurn))
    end

    local Player = self.mActor:GetActorTransform()
    local Previous = Player.Location
    local Position = self:MoveAlong(Previous, UnitX, self.mVelocity.x * Step)
    Position = self:MoveAlong(Position, UnitY, self.mVelocity.y * Step)
    local Horizontal = Vector.new(Position.x - Previous.x, Position.y - Previous.y, 0)
    self.mSpeed = Step > 0 and Horizontal:Length() / Step or 0
    self.mVerticalSpeed = self.mVerticalSpeed - Settings.Gravity * Step
    local VerticalDistance = self.mVerticalSpeed * Step

    if VerticalDistance > 0 then
        local Ceiling = World.Raycast(Position + UnitZ * 1.75, UnitZ, self.mActor)
        if Ceiling.IsHit and Ceiling.Distance < VerticalDistance + 0.05 then
            VerticalDistance = math.max(0, Ceiling.Distance - 0.05)
            self.mVerticalSpeed = 0
        end
    end

    Position = Position + UnitZ * VerticalDistance
    local Probe = Vector.new(Position.x, Position.y, math.max(Previous.z, Position.z) + Settings.StepHeight)
    local Ground = World.Raycast(Probe, Down, self.mActor)
    local GroundHeight = Ground.Position.z + 0.025
    local SnapDistance = self.mGrounded and Settings.StepHeight or 0
    local Reachable = Ground.IsHit and GroundHeight <= Previous.z + SnapDistance + 0.01
    local Contact = Position.z <= GroundHeight or (self.mGrounded and Position.z - GroundHeight <= Settings.StepHeight)
    self.mGrounded = self.mVerticalSpeed <= 0 and Reachable and Contact

    if self.mGrounded then
        Position.z = GroundHeight
        self.mVerticalSpeed = 0
    end

    if Position.z < -10 then
        self:ResetPlayer()
        return
    end

    Player.Location = Position
    Player.Rotation = Quat.new(UnitZ, self.mFacing) * self.mSpawn.Rotation
    self.mActor:SetActorTransform(Player)
end

function Controller:MoveAlong(Position, Direction, Distance)
    if math.abs(Distance) < 0.00001 then
        return Position
    end

    local TraceDirection = Direction * (Distance > 0 and 1 or -1)
    local Allowed = math.abs(Distance)
    for _, Height in ipairs(WallProbeHeights) do
        local Wall = World.Raycast(Position + UnitZ * Height, TraceDirection, self.mActor)
        if Wall.IsHit then
            Allowed = math.min(Allowed, math.max(0, Wall.Distance - 0.30))
        end
    end

    return Position + TraceDirection * Allowed
end

function Controller:UpdateAnimation()
    self.mMesh:SetAnimGraphFloat("Speed", self.mSpeed)
    self.mMesh:SetAnimGraphFloat("VerticalSpeed", self.mVerticalSpeed)
    self.mMesh:SetAnimGraphFloat("AimPitch", Clamp(self.mPitch / 0.58, -1, 1))
    self.mMesh:SetAnimGraphFloat("AimWeight", Clamp(1 - self.mSpeed / 5, 0.45, 1))
    self.mMesh:SetAnimGraphBool("IsInAir", not self.mGrounded)
    self.mMesh:SetAnimGraphBool("IsCrouching", self.mCrouch)
    self.mMesh:SetAnimGraphBool("IsSwimming", self.mSwim)
    self.mMesh:SetAnimGraphBool("IsAiming", self.mAim)
    self.mMesh:SetAnimGraphBool("WantsAction", self.mAction)
    self.mMesh:SetAnimGraphBool("WantsHit", self.mHit)
    self.mMesh:SetAnimGraphBool("IsDancing", self.mDance)
    self.mMesh:SetAnimGraphBool("IsDead", self.mDead)
end

function Controller:UpdateCamera(Step)
    local Rotation = (Quat.new(UnitZ, self.mYaw) * Quat.new(UnitY, -self.mPitch)):Normalized()
    local Target = self.mActor.Location + UnitZ * (self.mCrouch and 1.12 or 1.4)
    local Direction = Rotation:RotateVector(Back)
    local Obstacle = World.Raycast(Target, Direction, self.mActor)
    local Distance = Settings.CameraDistance * (self.mAim and 0.75 or 1)
    local Limit = Obstacle.IsHit and Clamp(Obstacle.Distance - 0.20, 0.20, Distance) or Distance
    self.mCameraArm = self.mCameraArm <= 0 and Limit or math.min(Limit, self.mCameraArm + Step * 8)

    local Camera = self.mCamera:GetActorTransform()
    Camera.Location = Target + Direction * self.mCameraArm
    Camera.Rotation = Rotation
    self.mCamera:SetActorTransform(Camera)
end

return function(Actor)
    return setmetatable({ mActor = Actor, mAlternatePunch = false, mAlternateHit = false }, Controller)
end
