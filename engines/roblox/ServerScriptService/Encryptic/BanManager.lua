-- Central violation tracking, strikes, kick/ban
local Players = game:GetService("Players")

local BanManager = {}
BanManager._strikes = {} :: { [Player]: { [string]: number } }
BanManager._totalStrikes = {} :: { [Player]: number }

export type ViolationAction = "warn" | "kick" | "ban"
export type ViolationInfo = {
	player: Player,
	guard: string,
	reason: string,
	severity: number,
}

local DEFAULT_MAX_STRIKES = 5
local maxStrikes = DEFAULT_MAX_STRIKES
local onViolation: ((ViolationInfo) -> ())? = nil
local onKick: ((Player, string) -> ())? = nil
local kicked: { [Player]: boolean } = {}

function BanManager.configure(opts: {
	maxStrikes: number?,
	onViolation: ((ViolationInfo) -> ())?,
	onKick: ((Player, string) -> ())?,
})
	maxStrikes = opts.maxStrikes or DEFAULT_MAX_STRIKES
	onViolation = opts.onViolation
	onKick = opts.onKick
end

function BanManager.record(violation: ViolationInfo): ViolationAction
	local player = violation.player
	if not player or not player.Parent or kicked[player] then
		return "warn"
	end

	local guardStrikes = BanManager._strikes[player]
	if not guardStrikes then
		guardStrikes = {}
		BanManager._strikes[player] = guardStrikes
	end

	guardStrikes[violation.guard] = (guardStrikes[violation.guard] or 0) + violation.severity
	BanManager._totalStrikes[player] = (BanManager._totalStrikes[player] or 0) + violation.severity

	warn(string.format(
		"[Encryptic] %s | %s | %s | strikes=%d",
		violation.guard,
		player.Name,
		violation.reason,
		BanManager._totalStrikes[player]
	))

	if onViolation then
		onViolation(violation)
	end

	local total = BanManager._totalStrikes[player]
	if total >= maxStrikes then
		kicked[player] = true
		if onKick then
			onKick(player, violation.reason)
		else
			player:Kick("[Encryptic] " .. violation.reason)
		end
		return "kick"
	end

	return "warn"
end

function BanManager.getStrikes(player: Player): number
	return BanManager._totalStrikes[player] or 0
end

function BanManager.reset(player: Player)
	BanManager._strikes[player] = nil
	BanManager._totalStrikes[player] = nil
	kicked[player] = nil
end

Players.PlayerRemoving:Connect(function(player)
	BanManager.reset(player)
end)

return BanManager
