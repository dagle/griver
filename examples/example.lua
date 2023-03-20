-- import lgi
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
  o.outer_padding = 5
  o.view_padding = 5
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
  local rotation = griver.Rotation.LEFT
  local output = get_output(out)

  out:tall_layout(view_count, width, height, output.main_count, output.view_padding,
  output.outer_padding, output.main_ratio, rotation, serial)

  out:commit_dimensions("[]=", serial)
end

-- Or define your own algorithm
local function tile_self(out, view_count, width, height, tags, serial)
  local output = get_output(out)

  width = width - 2 * output.outer_padding
  height = height - 2 * output.outer_padding;

  local main_size, stack_size, view_x, view_y, view_width, view_height
  if output.main_count == 0 then
    main_size  = 0
    stack_size = width
  elseif view_count <= output.main_count then
    main_size  = width;
    stack_size = 0;
  else
    main_size  = width * output.main_ratio
    stack_size = width - main_size
  end

  for i = 0, view_count do
    if i < output.main_count then
      view_x      = 0
      view_width  = main_size
      view_height = height / min(output.main_count, view_count)
      view_y      = i * view_height
    else
      view_x      = main_size
      view_width  = stack_size
      view_height = height / ( view_count - output.main_count)
      view_y      = (i - output.main_count) * view_height
    end
    out:push_view_dimensions(
    view_x + output.view_padding + output.outer_padding,
    view_y + output.view_padding + output.outer_padding,
    view_width - (2 * output.view_padding),
    view_height - (2 * output.view_padding),
    serial)
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

local function command(out, command, tags)
  local output = get_output(out)

  -- This is ugly-ish
  local n = find(command, "%s*main%-count%s+([+|-]?%d*)")
  if n then
    output.main_count = output.main_count + tonumber(n)
  end

  local n = find(command, "%s*view%-padding%s+(%d*)")
  if n then
    output.view_padding = output.main_count + tonumber(n)
  end

  local n = find(command, "%s*outer-padding%s+(%d*)")
  if n then
    output.outer_padding = output.main_count + tonumber(n)
  end

  local n = find(command, "%s*main-ratio%s+(%d*)")
  if n then
    output.main_ratio = clamp(tonumber(n), 0.1, 0.9)
  end

  local start, _ = string.find(command, "%s*reset%s*")
  if start then
    reset(output)
  end
end

local ctx = griver.new("griver")

ctx.on_output_add = function(_, output)
  local uid = output:get_uid()
  outputs[uid] = new_output(output)

  output.on_layout_demand = tile
  output.on_user_command = command
end

ctx.on_output_remove = function(_, output)
  local uid = output:get_uid()
  outputs[uid] = nil
end

local err = ctx:run()

print(string.format("Something bad happened: %s", err))
