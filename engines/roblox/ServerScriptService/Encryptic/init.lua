-- Encryptic Anti-Cheat — Roblox server bootstrap (advanced)
-- Place folder in ServerScriptService/Encryptic

local BanManager = require(script.Parent.BanManager)
local MovementGuard = require(script.Parent.MovementGuard)
local TeleportGuard = require(script.Parent.TeleportGuard)
local HumanoidGuard = require(script.Parent.HumanoidGuard)
local FlyGuard = require(script.Parent.FlyGuard)
local FireRateGuard = require(script.Parent.FireRateGuard)
local RemoteGuard = require(script.Parent.RemoteGuard)
local ExploitGuard = require(script.Parent.ExploitGuard)
local ToolGuard = require(script.Parent.ToolGuard)
local HitGuard = require(script.Parent.HitGuard)
local NoclipGuard = require(script.Parent.NoclipGuard)

local Encryptic = {}

export type Config = {
	maxSpeed: number?,
	maxTeleportDelta: number?,
	maxWalkSpeed: number?,
	maxJumpPower: number?,
	maxFireRate: number?,
	maxRemotePerSecond: number?,
	maxEquipPerSecond: number?,
	maxHitDistance: number?,
	maxAirTime: number?,
	maxStrikes: number?,
	enableFlyGuard: boolean?,
	enableNoclipGuard: boolean?,
	enableExploitGuard: boolean?,
	onKick: ((Player, string) -> ())?,
}

local function report(guard: string, severity: number)
	return function(player: Player, reason: string)
		BanManager.record({
			player = player,
			guard = guard,
			reason = reason,
			severity = severity,
		})
	end
end

function Encryptic.start(config: Config?)
	config = config or {}

	BanManager.configure({
		maxStrikes = config.maxStrikes or 5,
		onKick = config.onKick,
	})

	MovementGuard.start({
		maxSpeed = config.maxSpeed or 32,
		onViolation = report("MovementGuard", 1),
	})

	TeleportGuard.start({
		maxDelta = config.maxTeleportDelta or 80,
		onViolation = report("TeleportGuard", 2),
	})

	HumanoidGuard.start({
		maxWalkSpeed = config.maxWalkSpeed or 24,
		maxJumpPower = config.maxJumpPower or 60,
		onViolation = report("HumanoidGuard", 1),
	})

	if config.enableFlyGuard ~= false then
		FlyGuard.start({
			maxAirTime = config.maxAirTime or 4,
			onViolation = report("FlyGuard", 2),
		})
	end

	if config.enableNoclipGuard ~= false then
		NoclipGuard.start({
			onViolation = report("NoclipGuard", 2),
		})
	end

	FireRateGuard.start({
		maxFireRate = config.maxFireRate or 12,
		onViolation = report("FireRateGuard", 2),
	})

	RemoteGuard.start({
		maxRemotePerSecond = config.maxRemotePerSecond or 40,
		onViolation = report("RemoteGuard", 1),
	})

	ToolGuard.start({
		maxEquipPerSecond = config.maxEquipPerSecond or 8,
		onViolation = report("ToolGuard", 1),
	})

	HitGuard.start({
		maxHitDistance = config.maxHitDistance or 20,
		onViolation = report("HitGuard", 2),
	})

	if config.enableExploitGuard ~= false then
		ExploitGuard.start({
			onViolation = report("ExploitGuard", 2),
		})
	end

	print("[Encryptic] Advanced server guards active")
end

-- Re-export guard APIs for game integration
Encryptic.FireRateGuard = FireRateGuard
Encryptic.RemoteGuard = RemoteGuard
Encryptic.ToolGuard = ToolGuard
Encryptic.HitGuard = HitGuard
Encryptic.ExploitGuard = ExploitGuard
Encryptic.BanManager = BanManager

return Encryptic
