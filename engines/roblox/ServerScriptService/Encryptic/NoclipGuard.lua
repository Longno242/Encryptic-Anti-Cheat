local Players = game:GetService("Players")
local RunService = game:GetService("RunService")

local NoclipGuard = {}
local onViolation: ((Player, string) -> ())? = nil
local lastViolation: { [Player]: number } = {}
local violationCooldown = 0.75

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

local function restoreCollision(character: Model)
	for _, part in character:GetDescendants() do
		if part:IsA("BasePart") and part.Name ~= "Handle" then
			part.CanCollide = true
		end
	end
end

local function countNoclipParts(character: Model): number
	local count = 0
	for _, part in character:GetDescendants() do
		if part:IsA("BasePart") and not part.CanCollide and part.Name ~= "Handle" then
			count += 1
		end
	end
	return count
end

function NoclipGuard.start(opts: { onViolation: ((Player, string) -> ())? })
	onViolation = opts.onViolation

	RunService.Heartbeat:Connect(function()
		for _, player in Players:GetPlayers() do
			local character = player.Character
			if not character then
				continue
			end

			local noclipCount = countNoclipParts(character)
			if noclipCount >= 2 then
				report(player, string.format("Noclip exploit parts=%d", noclipCount))
				restoreCollision(character)
			end
		end
	end)

	Players.PlayerRemoving:Connect(function(player)
		lastViolation[player] = nil
	end)
end

return NoclipGuard
