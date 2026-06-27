--[[
	DemoClient.client.lua
	StarterPlayer > StarterPlayerScripts

	Encryptic demo client: keyboard shortcuts + test GUI.
	RightControl or Insert = toggle panel | ⚡ button top-right
]]

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local UserInputService = game:GetService("UserInputService")
local TweenService = game:GetService("TweenService")

local player = Players.LocalPlayer
local playerGui = player:WaitForChild("PlayerGui")

local remotes = ReplicatedStorage:WaitForChild("DemoRemotes", 15)
if not remotes then
	warn("[Demo Client] DemoRemotes not found — is DemoGame running?")
	return
end

local fireWeapon = remotes:WaitForChild("FireWeapon") :: RemoteEvent
local requestCoin = remotes:WaitForChild("RequestCoin") :: RemoteEvent
local testExploit = remotes:FindFirstChild("TestExploit") :: RemoteEvent?

if not testExploit then
	warn("[Demo Client] TestExploit remote missing — copy latest DemoGame.server.lua for fly/speed/TP mods")
end

local C = {
	panel = Color3.fromRGB(22, 24, 32),
	card = Color3.fromRGB(30, 33, 44),
	stroke = Color3.fromRGB(55, 60, 78),
	text = Color3.fromRGB(236, 238, 245),
	subtext = Color3.fromRGB(145, 152, 170),
	accent = Color3.fromRGB(99, 102, 241),
	accent2 = Color3.fromRGB(56, 189, 248),
	danger = Color3.fromRGB(239, 68, 68),
}

local toggleState = { speed = false, fly = false, noclip = false }
local firing = false
local panelOpen = false

local function tween(inst: Instance, props: { [string]: any }, time: number?)
	TweenService:Create(inst, TweenInfo.new(time or 0.2, Enum.EasingStyle.Quart, Enum.EasingDirection.Out), props):Play()
end

local function addCorner(parent: Instance, radius: number?)
	local c = Instance.new("UICorner")
	c.CornerRadius = UDim.new(0, radius or 10)
	c.Parent = parent
end

local function addStroke(parent: Instance, color: Color3?, transparency: number?)
	local s = Instance.new("UIStroke")
	s.Color = color or C.stroke
	s.Thickness = 1
	s.Transparency = transparency or 0.35
	s.ApplyStrokeMode = Enum.ApplyStrokeMode.Border
	s.Parent = parent
end

local function makeLabel(parent: Instance, text: string, size: number, color: Color3?, bold: boolean?)
	local l = Instance.new("TextLabel")
	l.BackgroundTransparency = 1
	l.Font = bold and Enum.Font.GothamBold or Enum.Font.GothamMedium
	l.TextSize = size
	l.TextColor3 = color or C.text
	l.TextXAlignment = Enum.TextXAlignment.Left
	l.Text = text
	l.Parent = parent
	return l
end

local function fireTest(action: string, enabled: boolean?)
	if not testExploit then
		warn("[Demo Client] Update DemoGame.server.lua — TestExploit remote not found")
		return
	end
	testExploit:FireServer(action, enabled)
end

local function makeButton(parent: Instance, text: string, color: Color3, layoutOrder: number, onClick: () -> ())
	local btn = Instance.new("TextButton")
	btn.Size = UDim2.new(1, 0, 0, 40)
	btn.BackgroundColor3 = color
	btn.BorderSizePixel = 0
	btn.AutoButtonColor = false
	btn.Font = Enum.Font.GothamBold
	btn.TextSize = 14
	btn.TextColor3 = C.text
	btn.Text = text
	btn.LayoutOrder = layoutOrder
	btn.Parent = parent
	addCorner(btn, 10)
	btn.MouseButton1Click:Connect(onClick)
	return btn
end

local function makeToggle(parent: Instance, label: string, guard: string, key: string, layoutOrder: number)
	local row = Instance.new("Frame")
	row.Size = UDim2.new(1, 0, 0, 52)
	row.BackgroundColor3 = C.card
	row.BorderSizePixel = 0
	row.LayoutOrder = layoutOrder
	row.Parent = parent
	addCorner(row, 12)
	addStroke(row)

	makeLabel(row, label, 14, C.text, true).Position = UDim2.fromOffset(14, 8)
	makeLabel(row, guard, 12, C.subtext).Position = UDim2.fromOffset(14, 28)

	local pill = Instance.new("TextButton")
	pill.Size = UDim2.fromOffset(54, 28)
	pill.Position = UDim2.new(1, -68, 0.5, -14)
	pill.BackgroundColor3 = C.stroke
	pill.Text = ""
	pill.AutoButtonColor = false
	pill.Parent = row
	addCorner(pill, 14)

	local knob = Instance.new("Frame")
	knob.Size = UDim2.fromOffset(22, 22)
	knob.Position = UDim2.fromOffset(3, 3)
	knob.BackgroundColor3 = C.text
	knob.BorderSizePixel = 0
	knob.Parent = pill
	addCorner(knob, 11)

	pill.MouseButton1Click:Connect(function()
		local next = not toggleState[key]
		toggleState[key] = next
		tween(pill, { BackgroundColor3 = next and C.accent or C.stroke })
		tween(knob, { Position = next and UDim2.fromOffset(29, 3) or UDim2.fromOffset(3, 3) })
		fireTest(key, next)
	end)
end

-- GUI (high DisplayOrder so other game UI doesn't hide it)
local gui = Instance.new("ScreenGui")
gui.Name = "EncrypticTestLab"
gui.ResetOnSpawn = false
gui.DisplayOrder = 999
gui.ZIndexBehavior = Enum.ZIndexBehavior.Sibling
gui.Parent = playerGui

local openBtn = Instance.new("TextButton")
openBtn.Name = "OpenButton"
openBtn.Size = UDim2.fromOffset(52, 52)
openBtn.Position = UDim2.new(1, -68, 0, 16)
openBtn.AnchorPoint = Vector2.new(0, 0)
openBtn.BackgroundColor3 = C.accent
openBtn.Text = "AC"
openBtn.TextSize = 18
openBtn.Font = Enum.Font.GothamBold
openBtn.TextColor3 = C.text
openBtn.AutoButtonColor = false
openBtn.Parent = gui
addCorner(openBtn, 14)
addStroke(openBtn, C.accent2, 0.3)

local panel = Instance.new("Frame")
panel.Name = "Panel"
panel.Size = UDim2.fromOffset(340, 520)
panel.Position = UDim2.new(1, -356, 0, 16)
panel.BackgroundColor3 = C.panel
panel.BorderSizePixel = 0
panel.Visible = false
panel.Parent = gui
addCorner(panel, 16)
addStroke(panel)

local header = Instance.new("Frame")
header.Size = UDim2.new(1, 0, 0, 88)
header.BackgroundColor3 = C.accent
header.BorderSizePixel = 0
header.Parent = panel
addCorner(header, 16)

local grad = Instance.new("UIGradient")
grad.Color = ColorSequence.new(C.accent, C.accent2)
grad.Rotation = 35
grad.Parent = header

makeLabel(header, "Encryptic Test Lab", 20, C.text, true).Position = UDim2.fromOffset(18, 18)
makeLabel(header, "Trigger anti-cheat guards", 13, Color3.fromRGB(230, 232, 255)).Position = UDim2.fromOffset(18, 46)

local body = Instance.new("ScrollingFrame")
body.Size = UDim2.new(1, -24, 1, -104)
body.Position = UDim2.fromOffset(12, 96)
body.BackgroundTransparency = 1
body.BorderSizePixel = 0
body.ScrollBarThickness = 4
body.AutomaticCanvasSize = Enum.AutomaticSize.Y
body.Parent = panel

local layout = Instance.new("UIListLayout")
layout.Padding = UDim.new(0, 10)
layout.SortOrder = Enum.SortOrder.LayoutOrder
layout.Parent = body

local function section(parent: Instance, text: string, order: number)
	local l = makeLabel(parent, text, 11, C.subtext, true)
	l.Size = UDim2.new(1, 0, 0, 16)
	l.LayoutOrder = order
end

section(body, "MOVEMENT MODS", 1)
makeToggle(body, "Speed Boost", "HumanoidGuard + MovementGuard", "speed", 2)
makeToggle(body, "Fly Mode", "FlyGuard + MovementGuard", "fly", 3)
makeToggle(body, "Noclip", "NoclipGuard", "noclip", 4)

section(body, "INSTANT ACTIONS", 10)
makeButton(body, "Teleport +200 studs", C.accent, 11, function() fireTest("teleport") end)
makeButton(body, "Mega Jump / HipHeight", C.accent, 12, function() fireTest("jump") end)
makeButton(body, "Velocity Burst", C.accent, 13, function() fireTest("velocity") end)

section(body, "REMOTE / COMBAT", 20)
makeButton(body, "Spam Coin Remote", C.danger, 21, function()
	for _ = 1, 25 do requestCoin:FireServer() end
end)
makeButton(body, "Start Rapid Fire", C.danger, 22, function()
	firing = true
	task.spawn(function()
		while firing do
			fireWeapon:FireServer()
			task.wait(0.05)
		end
	end)
end)
makeButton(body, "Stop Rapid Fire", Color3.fromRGB(71, 85, 105), 23, function()
	firing = false
end)

local footer = makeLabel(body, "RightControl / Insert = toggle", 11, C.subtext)
footer.Size = UDim2.new(1, 0, 0, 20)
footer.TextXAlignment = Enum.TextXAlignment.Center
footer.LayoutOrder = 99

local function setPanelOpen(open: boolean)
	panelOpen = open
	panel.Visible = open
	tween(openBtn, { BackgroundColor3 = open and C.accent2 or C.accent })
end

openBtn.MouseButton1Click:Connect(function()
	setPanelOpen(not panelOpen)
end)

local function onToggleInput(input: InputObject)
	if input.KeyCode == Enum.KeyCode.RightControl or input.KeyCode == Enum.KeyCode.Insert then
		setPanelOpen(not panelOpen)
	elseif input.KeyCode == Enum.KeyCode.F then
		firing = true
		task.spawn(function()
			while firing do
				fireWeapon:FireServer()
				task.wait(0.05)
			end
		end)
	elseif input.KeyCode == Enum.KeyCode.G then
		for _ = 1, 25 do
			requestCoin:FireServer()
		end
	end
end

UserInputService.InputBegan:Connect(function(input, processed)
	if processed then return end
	onToggleInput(input)
end)

UserInputService.InputEnded:Connect(function(input)
	if input.KeyCode == Enum.KeyCode.F then
		firing = false
	end
end)

player.CharacterRemoving:Connect(function()
	firing = false
end)

print("[Demo Client] GUI ready — click AC (top-right) or press RightControl / Insert")
