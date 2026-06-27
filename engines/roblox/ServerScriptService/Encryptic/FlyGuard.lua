local RunService = game:GetService("RunService")
local Workspace = game:GetService("Workspace")

local FlyGuard = {}
local maxAirTime = 4
local airTime: { [Player]: number } = {}
local lastViolation: { [Player]: number } = {}
local violationCooldown = 0.75
local onViolation: ((Player, string) -> ())? = nil

local ALLOWED_STATES = {
	[Enum.HumanoidStateType.Jumping] = true,
	[Enum.HumanoidStateType.Freefall] = true,
	[Enum.HumanoidStateType.Climbing] = true,
	[Enum.HumanoidStateType.Swimming] = true,
}

local function report(player: Player, reason: string)
	if not onViolation then
		return
	end
	local now = os.clock()
	if (now - (lastViolation[player] or 0)) < violationCooldown then
		return
	end
	lastViolation[player] = now
	onViolation(player, reason)
end

local function isGrounded(character: Model, root: BasePart): boolean
	local params = RaycastParams.new()
	params.FilterDescendantsInstances = { character }
	params.FilterType = Enum.RaycastFilterType.Exclude
	return Workspace:Raycast(root.Position, Vector3.new(0, -4, 0), params) ~= nil
end

function FlyGuard.start(opts: {
	maxAirTime: number?,
	onViolation: ((Player, string) -> ())?,
})
	maxAirTime = opts.maxAirTime or maxAirTime
	onViolation = opts.onViolation

	RunService.Heartbeat:Connect(function(dt)
		for _, player in game:GetService("Players"):GetPlayers() do
			local character = player.Character
			local root = character and character:FindFirstChild("HumanoidRootPart") :: BasePart?
			local humanoid = character and character:FindFirstChildOfClass("Humanoid")
			if not root or not humanoid then
				airTime[player] = 0
				continue
			end

			if humanoid.SeatPart then
				airTime[player] = 0
				continue
			end

			-- Classic executor fly (test GUI uses this too)
			if humanoid.PlatformStand then
				report(player, "Fly exploit PlatformStand")
				humanoid.PlatformStand = false
				root.AssemblyLinearVelocity = Vector3.new(
					root.AssemblyLinearVelocity.X,
					math.min(root.AssemblyLinearVelocity.Y, -40),
					root.AssemblyLinearVelocity.Z
				)
				airTime[player] = 0
				continue
			end

			local state = humanoid:GetState()
			if ALLOWED_STATES[state] then
				airTime[player] = 0
				continue
			end

			local grounded = isGrounded(character, root)
			if grounded then
				airTime[player] = 0
				continue
			end

			local velocity = root.AssemblyLinearVelocity
			local verticalSpeed = velocity.Y
			local horizontalSpeed = Vector3.new(velocity.X, 0, velocity.Z).Magnitude

			-- Hover fly: nearly still in mid-air for a long time (not a normal jump arc)
			local hovering = math.abs(verticalSpeed) < 4 and horizontalSpeed < humanoid.WalkSpeed + 6
			if hovering then
				airTime[player] = (airTime[player] or 0) + dt
				if airTime[player] > maxAirTime then
					report(player, string.format("Fly exploit hover airTime=%.1fs", airTime[player]))
					root.AssemblyLinearVelocity = Vector3.new(velocity.X, -80, velocity.Z)
					airTime[player] = 0
				end
			else
				airTime[player] = 0
			end
		end
	end)

	game:GetService("Players").PlayerRemoving:Connect(function(player)
		airTime[player] = nil
		lastViolation[player] = nil
	end)
end

return FlyGuard
