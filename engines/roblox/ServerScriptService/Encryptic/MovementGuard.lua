local Players = game:GetService("Players")
local RunService = game:GetService("RunService")

local MovementGuard = {}
local maxSpeed = 32
local onViolation: ((Player, string) -> ())? = nil
local lastViolation: { [Player]: number } = {}
local violationCooldown = 1.0

local function horizontalSpeed(velocity: Vector3): number
	return Vector3.new(velocity.X, 0, velocity.Z).Magnitude
end

local function clampHorizontal(root: BasePart, limit: number)
	local velocity = root.AssemblyLinearVelocity
	local flat = Vector3.new(velocity.X, 0, velocity.Z)
	if flat.Magnitude <= limit then
		return
	end

	local clamped = flat.Unit * limit
	root.AssemblyLinearVelocity = Vector3.new(clamped.X, velocity.Y, clamped.Z)
end

function MovementGuard.start(opts: number | { maxSpeed: number?, onViolation: ((Player, string) -> ())? })
	if typeof(opts) == "number" then
		maxSpeed = opts
	else
		maxSpeed = opts.maxSpeed or maxSpeed
		onViolation = opts.onViolation
	end

	RunService.Heartbeat:Connect(function()
		local now = os.clock()
		for _, player in Players:GetPlayers() do
			local character = player.Character
			local root = character and character:FindFirstChild("HumanoidRootPart") :: BasePart?
			if not root then
				continue
			end

			local humanoid = character:FindFirstChildOfClass("Humanoid")
			local walkSpeed = humanoid and humanoid.WalkSpeed or 16
			-- Allow sprint/jump momentum; only flag sustained horizontal speed well above walk speed
			local limit = math.max(maxSpeed, walkSpeed * 2 + 8)
			local speed = horizontalSpeed(root.AssemblyLinearVelocity)

			if speed > limit then
				clampHorizontal(root, limit)

				if onViolation and (now - (lastViolation[player] or 0)) >= violationCooldown then
					lastViolation[player] = now
					onViolation(player, string.format("Speed exploit speed=%.1f max=%.1f", speed, limit))
				end
			end
		end
	end)

	Players.PlayerRemoving:Connect(function(player)
		lastViolation[player] = nil
	end)
end

return MovementGuard
