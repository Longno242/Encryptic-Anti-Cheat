local Players = game:GetService("Players")
local RunService = game:GetService("RunService")

local HumanoidGuard = {}
local maxWalkSpeed = 24
local maxJumpPower = 60
local maxJumpHeight = 12
local maxHipHeight = 8
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

local function clampHumanoid(player: Player, humanoid: Humanoid)
	if humanoid.WalkSpeed > maxWalkSpeed then
		report(player, "WalkSpeed tamper: " .. humanoid.WalkSpeed)
		humanoid.WalkSpeed = maxWalkSpeed
	end

	if humanoid.UseJumpPower and humanoid.JumpPower > maxJumpPower then
		report(player, "JumpPower tamper: " .. humanoid.JumpPower)
		humanoid.JumpPower = maxJumpPower
	end

	if humanoid.JumpHeight > maxJumpHeight then
		report(player, "JumpHeight tamper: " .. humanoid.JumpHeight)
		humanoid.JumpHeight = maxJumpHeight
	end

	if humanoid.HipHeight > maxHipHeight then
		report(player, "HipHeight tamper: " .. humanoid.HipHeight)
		humanoid.HipHeight = maxHipHeight
	end
end

local function bindCharacter(player: Player, character: Model)
	local humanoid = character:WaitForChild("Humanoid", 5) :: Humanoid?
	if not humanoid then
		return
	end

	clampHumanoid(player, humanoid)

	local props = { "WalkSpeed", "JumpPower", "JumpHeight", "HipHeight" }
	for _, prop in props do
		humanoid:GetPropertyChangedSignal(prop):Connect(function()
			clampHumanoid(player, humanoid)
		end)
	end
end

function HumanoidGuard.start(opts: {
	maxWalkSpeed: number?,
	maxJumpPower: number?,
	maxJumpHeight: number?,
	onViolation: ((Player, string) -> ())?,
})
	maxWalkSpeed = opts.maxWalkSpeed or maxWalkSpeed
	maxJumpPower = opts.maxJumpPower or maxJumpPower
	maxJumpHeight = opts.maxJumpHeight or maxJumpHeight
	onViolation = opts.onViolation

	local function onPlayer(player: Player)
		player.CharacterAdded:Connect(function(character)
			bindCharacter(player, character)
		end)
		if player.Character then
			bindCharacter(player, player.Character)
		end
	end

	for _, player in Players:GetPlayers() do
		onPlayer(player)
	end
	Players.PlayerAdded:Connect(onPlayer)

	-- Backup poll — some exploits bypass PropertyChanged
	RunService.Heartbeat:Connect(function()
		for _, player in Players:GetPlayers() do
			local humanoid = player.Character and player.Character:FindFirstChildOfClass("Humanoid")
			if humanoid then
				clampHumanoid(player, humanoid)
			end
		end
	end)

	Players.PlayerRemoving:Connect(function(player)
		lastViolation[player] = nil
	end)
end

return HumanoidGuard
