local RunService = game:GetService("RunService")

local TeleportGuard = {}
local maxDelta = 80
local lastPos: { [Model]: Vector3 } = {}
local lastTime: { [Model]: number } = {}
local lastViolation: { [Player]: number } = {}
local violationCooldown = 0.75
local onViolation: ((Player, string) -> ())? = nil

function TeleportGuard.start(opts: { maxDelta: number?, onViolation: ((Player, string) -> ())? })
	maxDelta = opts.maxDelta or maxDelta
	onViolation = opts.onViolation

	RunService.Heartbeat:Connect(function()
		for _, player in game:GetService("Players"):GetPlayers() do
			local character = player.Character
			local root = character and character:FindFirstChild("HumanoidRootPart") :: BasePart?
			if not root then
				continue
			end

			local now = os.clock()
			local prev = lastPos[character]
			local prevT = lastTime[character]

			if prev and prevT then
				local dt = math.max(now - prevT, 1 / 60)
				local delta = (root.Position - prev).Magnitude
				local humanoid = character:FindFirstChildOfClass("Humanoid")
				local allowed = (humanoid and humanoid.WalkSpeed or 16) * dt * 3 + 12

				if delta > math.max(maxDelta, allowed) then
					root.CFrame = CFrame.new(prev)
					local now = os.clock()
					if onViolation and (now - (lastViolation[player] or 0)) >= violationCooldown then
						lastViolation[player] = now
						onViolation(player, string.format("Teleport/noclip delta=%.1f allowed=%.1f", delta, allowed))
					end
				end
			end

			lastPos[character] = root.Position
			lastTime[character] = now
		end
	end)
end

return TeleportGuard
