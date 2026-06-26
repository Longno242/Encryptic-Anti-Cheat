local FireRateGuard = {}
local maxRate = 12
local lastFire: { [Player]: { number } } = {}

function FireRateGuard.start(ratePerSecond: number)
	maxRate = ratePerSecond

	-- Example API: game.BindableEvent or your gun system calls FireRateGuard.recordShot(player)
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
		warn("[Encryptic] Fire rate violation", player.Name, #history)
		return false
	end

	return true
end

return FireRateGuard
