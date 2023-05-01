#!/bin/luajit
local lgi = require("lgi")
local griver = lgi.require("Griver", 0.1)

-- bring local copies, for non-jit versions of lua
local min = math.min
local max = math.max

-- Our screens
local outputs = {}

-- Sets the defaults
local function reset(o)
  o.main_count = 1
  o.main_ratio = 0.6
  o.outer_padding = 0
  o.view_padding = 0
  o.rotation = griver.Rotation.LEFT
end

local function new_output(output)
  local o = {
    output = output,
  }
  reset(o)
  return o
end

local function get_output(output)
  local uid = output:get_uid()
  return outputs[uid]
end

-- tile using the a pre-defined algorithm
local function tile(out, view_count, width, height, tags, serial)
  local output = get_output(out)
  local rotation = output.rotation

  out:tall_layout(view_count, width, height, output.main_count, output.view_padding,
  output.outer_padding, output.main_ratio, rotation, serial)

  out:commit_dimensions("[]=", serial)
end

-- Or define your own algorithm
local function tile_self(out, view_count, width, height, tags, serial)
  local rotation = griver.Rotation

  if view_count <= 0 then
    return
  end

  local output = get_output(out)

  local lmain_count = min(output.main_count, view_count)
	local secondary_count = view_count - lmain_count

  local view_padding = output.view_padding
  local outer_padding = output.outer_padding

  local usable_width
  local usable_height

  if (output.rotation == rotation.LEFT) or (output.rotation == rotation.RIGHT) then
    usable_width = width - 2 * outer_padding
    usable_height = height - 2 * outer_padding
  else
    usable_height = width - 2 * outer_padding
    usable_width = height - 2 * outer_padding
  end

  local main_count = output.main_count

	local main_width, main_height, main_height_rem;
	local secondary_width, secondary_height, secondary_height_rem;

  if secondary_count > 0 then
    main_width = math.floor(output.main_ratio * usable_width)
    main_height = math.floor(usable_height / main_count)
    main_height_rem = usable_height % main_count

    secondary_width = usable_width - main_width
    secondary_height = math.floor(usable_height / secondary_count)
    secondary_height_rem = usable_height % secondary_count
  else
    main_width = usable_width
    main_height = math.floor(usable_height / main_count)
    main_height_rem = usable_height % main_count
  end

  for i = 0, view_count-1 do
    local x, y, lwidth, lheight

    if i < main_count then
      x = 0
      y = i * main_count
      if i > 0 then
        y = y + main_height_rem
      end
      lwidth = main_width
      lheight = main_height
      if i == 0 then
        lheight = lheight + main_height_rem
      end
    else
      x = main_width
      y = (i - main_count) * secondary_height
      if i > main_count then
        y = y + secondary_height_rem
      end
      lwidth = secondary_width
      lheight = secondary_height
      if i == main_count then
        lheight = lheight + secondary_height_rem
      end
    end

    x = x + view_padding
    y = y + view_padding
    lwidth = lwidth - 2 * view_padding
    lheight = lheight - 2 * view_padding

    if output.rotation == rotation.LEFT then
      out:push_view_dimensions(
        x + outer_padding,
        y + outer_padding,
        lwidth,
        lheight,
        serial)
    elseif output.rotation == rotation.RIGHT then
      out:push_view_dimensions(
        width - lwidth - x + outer_padding,
        y + outer_padding,
        lwidth,
        lheight,
        serial)
    elseif output.rotation == rotation.TOP then
      out:push_view_dimensions(
        y + outer_padding,
        x + outer_padding,
        lheight,
        lwidth,
        serial)
    elseif output.rotation == rotation.BOTTOM then
      out:push_view_dimensions(
        y + outer_padding,
        usable_width - lwidth - x + outer_padding,
        lheight,
        lwidth,
        serial)
    end
  end
  out:commit_dimensions("[]=", serial)
end

local function clamp(val, low, upp)
  return min(max(low, upp), max(min(low, upp), val))
end

local function find(command, match)
  local _, _, n = string.find(command, match)
  return n
end

local function command(out, cmd, tags)
  local output = get_output(out)

  -- This is ugly-ish
  local n = find(cmd, "main%-count%s+([-+]%d)")
  if n then
    output.main_count = output.main_count + tonumber(n)
  end

  local n = find(cmd, "%s*view%-padding%s+(%d*)")
  if n then
    output.view_padding = output.main_count + tonumber(n)
  end

  local n = find(cmd, "%s*outer%-padding%s+(%d*)")
  if n then
    output.outer_padding = output.main_count + tonumber(n)
  end

  local n = find(cmd, "main%-ratio%s+([-+]?%d+%.%d*)")
  if n then
    output.main_ratio = clamp(output.main_ratio + tonumber(n), 0.1, 0.9)
  end

  local rotation = find(cmd, "main%-location%s+(%a+)")
  if rotation then
    rotation = rotation:upper()
    output.rotation = griver.Rotation[rotation] or output.rotation
  end


  local start, _ = string.find(cmd, "%s*reset%s*")
  if start then
    reset(output)
  end
end

local ctx = griver.Context.new("rivertile")

ctx.on_output_add = function(_, output)
  local uid = output:get_uid()
  outputs[uid] = new_output(output)

  -- output.on_layout_demand = tile
  output.on_layout_demand = tile_self
  output.on_user_command = command
end

ctx.on_output_remove = function(_, output)
  local uid = output:get_uid()
  outputs[uid] = nil
end

local err = ctx:run()

if err then
  print(string.format("Something bad happened: %s", err))
end
