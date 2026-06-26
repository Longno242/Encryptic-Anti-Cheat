-- Encryptic Anti-Cheat — Roblox server bootstrap
-- Place in ServerScriptService/Encryptic/init.lua

local MovementGuard = require(script.MovementGuard)
local FireRateGuard = require(script.FireRateGuard)
local RemoteGuard = require(script.RemoteGuard)

local Encryptic = {}

export type Config = {
	maxSpeed: number?,
	maxFireRate: number?,
	maxRemotePerSecond: number?,
}

function Encryptic.start(config: Config?)
	config = config or {}
	MovementGuard.start(config.maxSpeed or 32)
	FireRateGuard.start(config.maxFireRate or 12)
	RemoteGuard.start(config.maxRemotePerSecond or 40)
	print("[Encryptic] Server guards active")
end

return Encryptic
