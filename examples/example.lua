#!/bin/luajit
local lgi = require "lgi"
local griver = lgi.require("Griver", 0.1)

-- bring local copies, for non-jit versions of lua
local min = math.min
local max = math.max

-- Our screens
local outputs = {}

local function mktags()
  local tags = {}
  -- create 9 tags for each output
  for _ = 1, 9 do
    local tag = {
      main_count = 1,
      main_ratio = 0.6,
      outer_padding = 0,
      view_padding = 0,
      rotation = griver.Rotation.LEFT,
    }
    table.insert(tags, tag)
  end
  return tags
end

-- Sets the defaults
local function reset(o)
  o.tags = mktags()
end

local function new_output(output)
  local o = {
    output = output,
  }
  reset(o)
  return o
end

local function get_output(out)
  local uid = out:get_uid()
  return outputs[uid]
end

local function get_tag(out, tags)
  local output = get_output(out)
  return output.tags[griver.first_set_bit_pos(tags)]
end

-- tile using the a pre-defined algorithm
local function tile(out, view_count, width, height, tags, serial)
  local tag = get_tag(out, tags)
  local rotation = tag.rotation

  out:tall_layout(
    view_count,
    width,
    height,
    tag.main_count,
    tag.view_padding,
    tag.outer_padding,
    tag.main_ratio,
    rotation,
    serial
  )

  out:commit_dimensions("[]=", serial)
end

-- Or define your own algorithm
local function tile_self(
  out,
  view_count,
  width,
  height,
  main_count,
  view_padding,
  outer_padding,
  main_ratio,
  rotation,
  serial
)
  if view_count <= 0 then
    return
  end

  local lmain_count = min(main_count, view_count)
  local secondary_count = view_count - lmain_count

  local usable_width
  local usable_height

  if (rotation == rotation.LEFT) or (rotation == rotation.RIGHT) then
    usable_width = width - 2 * outer_padding
    usable_height = height - 2 * outer_padding
  else
    usable_height = width - 2 * outer_padding
    usable_width = height - 2 * outer_padding
  end

  local main_width, main_height, main_height_rem
  local secondary_width, secondary_height, secondary_height_rem

  if secondary_count > 0 then
    main_width = math.floor(main_ratio * usable_width)
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

  for i = 0, view_count - 1 do
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

    if rotation == rotation.LEFT then
      out:push_view_dimensions(x + outer_padding, y + outer_padding, lwidth, lheight, serial)
    elseif rotation == rotation.RIGHT then
      out:push_view_dimensions(width - lwidth - x + outer_padding, y + outer_padding, lwidth, lheight, serial)
    elseif rotation == rotation.TOP then
      out:push_view_dimensions(y + outer_padding, x + outer_padding, lheight, lwidth, serial)
    elseif rotation == rotation.BOTTOM then
      out:push_view_dimensions(y + outer_padding, usable_width - lwidth - x + outer_padding, lheight, lwidth, serial)
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

local function split(str)
  local buf = {}
  for word in str:gmatch "%S+" do
    table.insert(buf, word)
  end
  return buf
end

local function command(out, cmd, tags)
  local output = get_output(out)
  local tag = get_tag(out, tags)

  local args = split(cmd)

  if not args[1] then
    return
  end

  if args[1] == "main-count" and args[2] then
    local num = tonumber(args[2])
    if num then
      tag.main_count = tag.main_count + num
    end
  end

  if args[1] == "view-padding" and args[2] then
    local num = tonumber(args[2])
    if num then
      tag.view_padding = tag.main_count + num
    end
  end

  if args[1] == "outer-padding" and args[2] then
    local num = tonumber(args[2])
    if num then
      tag.outer_padding = tag.outer_padding + num
    end
  end

  if args[1] == "main-ratio" and args[2] then
    local num = tonumber(args[2])
    if num then
      tag.main_ratio = clamp(tag.main_ratio + tonumber(num), 0.1, 0.9)
    end
  end

  local rotation = find(cmd, "main%-location%s+(%a+)")
  if rotation then
    rotation = rotation:upper()
    tag.rotation = griver.Rotation[rotation] or tag.rotation
  end

  if args[1] == "reset" then
    reset(output)
  end
end

local ctx = griver.Context.new "rivertile"

ctx.on_output_add = function(_, output)
  local uid = output:get_uid()
  outputs[uid] = new_output(output)

  -- output.on_layout_demand = tile
  output.on_layout_demand = tile
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
