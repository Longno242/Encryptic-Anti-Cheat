local Players = game:GetService("Players")
local RunService = game:GetService("RunService")

local MovementGuard = {}
local maxSpeed = 32

function MovementGuard.start(limit: number)
	maxSpeed = limit
	RunService.Heartbeat:Connect(function()
		for _, player in Players:GetPlayers() do
			local character = player.Character
			local root = character and character:FindFirstChild("HumanoidRootPart") :: BasePart?
			if root then
				if root.AssemblyLinearVelocity.Magnitude > maxSpeed then
					warn("[Encryptic] Speed violation", player.Name, root.AssemblyLinearVelocity.Magnitude)
					root.AssemblyLinearVelocity = root.AssemblyLinearVelocity.Unit * maxSpeed
				end
			end
		end
	end)
end

return MovementGuard
