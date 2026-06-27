--[[
	DemoGame.server.lua
	Put this in ServerScriptService next to the Encryptic folder.

	ServerScriptService/
	  Encryptic/          (copy whole folder from engines/roblox/ServerScriptService/Encryptic)
	  DemoGame.server.lua (this file)
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local RunService = game:GetService("RunService")

local Encryptic = require(script.Parent.Encryptic.init)

-- Start anti-cheat (server-side only — this is the important part)
Encryptic.start({
	maxSpeed = 48,
	maxWalkSpeed = 24,
	maxFireRate = 5, -- low on purpose so spam-clicking triggers a violation quickly
	maxRemotePerSecond = 15,
	maxStrikes = 3, -- kick after 3 strike points for demo
	enableFlyGuard = true,
	enableExploitGuard = true,
})

print("[Demo] Encryptic started. Watch the Output window for violation messages.")

-- Remotes used by the demo client
local remotesFolder = ReplicatedStorage:FindFirstChild("DemoRemotes")
if not remotesFolder then
	remotesFolder = Instance.new("Folder")
	remotesFolder.Name = "DemoRemotes"
	remotesFolder.Parent = ReplicatedStorage
end

local function getOrCreateRemote(name: string): RemoteEvent
	local existing = remotesFolder:FindFirstChild(name)
	if existing and existing:IsA("RemoteEvent") then
		return existing
	end
	local remote = Instance.new("RemoteEvent")
	remote.Name = name
	remote.Parent = remotesFolder
	return remote
end

local fireWeapon = getOrCreateRemote("FireWeapon")
local requestCoin = getOrCreateRemote("RequestCoin")
local testExploit = getOrCreateRemote("TestExploit")

-- Server-side cheat simulation for Encryptic test GUI (demo only)
local flyLoops: { [Player]: RBXScriptConnection } = {}
local noclipParts: { [Player]: { BasePart } } = {}

local function stopFly(player: Player)
	local conn = flyLoops[player]
	if conn then
		conn:Disconnect()
		flyLoops[player] = nil
	end
	local humanoid = player.Character and player.Character:FindFirstChildOfClass("Humanoid")
	if humanoid then
		humanoid.PlatformStand = false
	end
end

local function setNoclip(player: Player, enabled: boolean)
	local character = player.Character
	if not character then
		return
	end

	if not enabled then
		for _, part in noclipParts[player] or {} do
			if part.Parent then
				part.CanCollide = true
			end
		end
		noclipParts[player] = nil
		return
	end

	noclipParts[player] = {}
	for _, part in character:GetDescendants() do
		if part:IsA("BasePart") then
			part.CanCollide = false
			table.insert(noclipParts[player], part)
		end
	end
end

testExploit.OnServerEvent:Connect(function(player: Player, action: string, enabled: boolean?)
	local character = player.Character
	local humanoid = character and character:FindFirstChildOfClass("Humanoid")
	local root = character and character:FindFirstChild("HumanoidRootPart") :: BasePart?
	if not humanoid or not root then
		return
	end

	if action == "speed" then
		humanoid.WalkSpeed = if enabled then 100 else 16
	elseif action == "fly" then
		stopFly(player)
		if enabled then
			humanoid.PlatformStand = true
			flyLoops[player] = RunService.Heartbeat:Connect(function()
				if not root.Parent then
					stopFly(player)
					return
				end
				root.AssemblyLinearVelocity = Vector3.new(0, 42, 0)
			end)
		end
	elseif action == "noclip" then
		setNoclip(player, enabled == true)
	elseif action == "teleport" then
		root.CFrame = root.CFrame + root.CFrame.LookVector * 200
	elseif action == "jump" then
		humanoid.UseJumpPower = true
		humanoid.JumpPower = 150
		humanoid.JumpHeight = 20
		humanoid.HipHeight = 12
	elseif action == "velocity" then
		root.AssemblyLinearVelocity = root.CFrame.LookVector * 120 + Vector3.new(0, 30, 0)
	end
end)

Players.PlayerRemoving:Connect(function(player)
	stopFly(player)
	noclipParts[player] = nil
end)

-- Example: gun fire rate validation
Encryptic.RemoteGuard.wrapRemoteEvent(fireWeapon, function(player: Player)
	if not Encryptic.FireRateGuard.recordShot(player) then
		return -- fire rate guard already logged a violation
	end

	-- Normal game logic would go here (raycast, damage, etc.)
	print("[Demo] Valid shot from", player.Name)
end)

-- Example: remote spam protection (spam RequestCoin in Studio to trigger RemoteGuard)
requestCoin.OnServerEvent:Connect(function(player: Player)
	if not Encryptic.RemoteGuard.track(player) then
		return
	end

	if not Encryptic.ExploitGuard.validateRemote(player, requestCoin) then
		return
	end

	print("[Demo] Coin granted to", player.Name, "| strikes:", Encryptic.BanManager.getStrikes(player))
end)

-- Example: tool equip tracking
local function onCharacterAdded(player: Player, character: Model)
	character.ChildAdded:Connect(function(child)
		if child:IsA("Tool") then
			if not Encryptic.ToolGuard.trackEquip(player) then
				child:Destroy()
			end
		end
	end)
end

Players.PlayerAdded:Connect(function(player)
	player.CharacterAdded:Connect(function(character)
		onCharacterAdded(player, character)
	end)

	print(string.format(
		"[Demo] %s joined. Open test panel (RightControl). Strikes kick at 3.",
		player.Name
	))
end)

for _, player in Players:GetPlayers() do
	if player.Character then
		onCharacterAdded(player, player.Character)
	end
end
