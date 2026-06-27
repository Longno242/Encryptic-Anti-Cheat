local FireRateGuard = {}
local maxRate = 12
local lastFire: { [Player]: { number } } = {}
local onViolation: ((Player, string) -> ())? = nil

function FireRateGuard.start(opts: number | { maxFireRate: number?, onViolation: ((Player, string) -> ())? })
	if typeof(opts) == "number" then
		maxRate = opts
	else
		maxRate = opts.maxFireRate or maxRate
		onViolation = opts.onViolation
	end
end

function FireRateGuard.recordShot(player: Player): boolean
	local now = os.clock()
	local history = lastFire[player]
	if not history then
		history = {}
		lastFire[player] = history
	end

	table.insert(history, now)
	while #history > 0 and now - history[1] > 1 do
		table.remove(history, 1)
	end

	if #history > maxRate then
		if onViolation then
			onViolation(player, string.format("Fire rate exploit shots=%d max=%d", #history, maxRate))
		end
		return false
	end

	return true
end

return FireRateGuard
