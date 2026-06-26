local Players = game:GetService("Players")

local RemoteGuard = {}
local maxPerSecond = 40
local counts: { [Player]: { number } } = {}

function RemoteGuard.start(limit: number)
	maxPerSecond = limit

	Players.PlayerRemoving:Connect(function(player)
		counts[player] = nil
	end)
end

function RemoteGuard.track(player: Player): boolean
	local now = os.clock()
	local history = counts[player]
	if not history then
		history = {}
		counts[player] = history
	end

	table.insert(history, now)
	while #history > 0 and now - history[1] > 1 do
		table.remove(history, 1)
	end

	if #history > maxPerSecond then
		warn("[Encryptic] Remote spam", player.Name)
		return false
	end

	return true
end

return RemoteGuard
