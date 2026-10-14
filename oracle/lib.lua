-- oracle/lib.lua — shared helpers for oracle scenarios (design §4).
-- Scenarios start with: dofile(os.getenv("ORACLE_LIB"))
--
-- API:
--   check(name, cond, detail)   record one pass/fail check
--   record(key, value)          attach a key/value fact (e.g. app version)
--   wait_ms(ms, fn, on_done)    defer loop: fn() each cycle until ms elapsed,
--                               then on_done() if given
--   start(mode, body)          run body once synced, event-based (no fixed
--                               warm-up timers):
--                                 "device"  plugin traced `log connected`, i.e. the
--                                           fake device clock just started (lua t ~ script t)
--                                 "settled" 10 consecutive defer cycles < 100 ms
--                                           (startup-dialog stalls are over)
--   read_trace()                plugin trace as a list of "<ms> <msg>" lines;
--                               records a failing check + returns nil if unreadable
--   find_line(lines, needle, from)  index of first line >= from containing needle
--                               (plain match), or 0
--   window(samples, tmin, tmax) first/last index of samples (field .t, ms)
--                               inside [tmin, tmax], or nil
--   finish()                    write probe.json to $ORACLE_OUT, then quit REAPER

local oracle = {}
local checks = {}
local info = {}

function oracle.check(name, cond, detail)
  table.insert(checks, { name = name, pass = cond and true or false,
                         detail = detail or "" })
end

function oracle.record(key, value)
  info[key] = tostring(value)
end

function oracle.wait_ms(ms, fn, on_done)
  local t0 = reaper.time_precise()
  local function step()
    if fn then
      local ok, err = pcall(fn)
      if not ok then
        oracle.check("scenario step error", false, tostring(err))
        return oracle.finish()
      end
    end
    if (reaper.time_precise() - t0) * 1000 >= ms then
      if on_done then
        local ok, err = pcall(on_done)
        if not ok then
          oracle.check("scenario callback error", false, tostring(err))
          oracle.finish()
        end
      end
    else
      reaper.defer(step)
    end
  end
  reaper.defer(step)
end

-- Minimal JSON encoder (strings/numbers/bools/arrays/tables).
local function is_array(t)
  local n = 0
  for k in pairs(t) do
    if type(k) ~= "number" then return false end
    n = n + 1
  end
  return n == #t
end

local function encode(v)
  local t = type(v)
  if t == "string" then
    return string.format(
      '"%s"',
      v:gsub('[%c"\\]', function(c)
        local m = { ['"'] = '\\"', ['\\'] = '\\\\',
                    ['\n'] = '\\n', ['\r'] = '\\r', ['\t'] = '\\t' }
        return m[c] or string.format('\\u%04x', c:byte())
      end))
  elseif t == "number" then
    return string.format("%.17g", v)
  elseif t == "boolean" then
    return v and "true" or "false"
  elseif t == "table" then
    local parts = {}
    if is_array(v) then
      for i = 1, #v do parts[i] = encode(v[i]) end
      return "[" .. table.concat(parts, ",") .. "]"
    else
      local keys = {}
      for k in pairs(v) do table.insert(keys, k) end
      table.sort(keys)
      for _, k in ipairs(keys) do
        table.insert(parts, encode(k) .. ":" .. encode(v[k]))
      end
      return "{" .. table.concat(parts, ",") .. "}"
    end
  end
  return "null"
end

function oracle.finish()
  local out_dir = os.getenv("ORACLE_OUT")
  if not out_dir then
    reaper.ShowConsoleMsg("oracle: ORACLE_OUT env not set\n")
    return
  end
  -- Write to a temp name then rename: the harness kills REAPER as soon as
  -- probe.json exists, so it must never see a half-written file.
  local tmp = out_dir .. "/probe.json.tmp"
  local f = io.open(tmp, "w")
  if not f then
    reaper.ShowConsoleMsg("oracle: cannot write " .. tmp .. "\n")
    return
  end
  f:write(encode({ checks = checks, info = info }) .. "\n")
  f:close()
  os.rename(tmp, out_dir .. "/probe.json")
  -- The harness kills REAPER once probe.json appears (a dirty project's
  -- "save changes?" Query would otherwise block Quit). Quit anyway for
  -- manual runs.
  reaper.Main_OnCommand(40004, 0) -- Quit REAPER
end

-- Also expose the names unqualified so scenarios can just call check/record/finish.
check = oracle.check
record = oracle.record
wait_ms = oracle.wait_ms
finish = oracle.finish

-- Run a scenario body; any Lua error becomes a failing check + immediate
-- finish() (instead of a silent 60 s timeout).
function oracle.run(body)
  local ok, err = xpcall(body, debug.traceback)
  if not ok then
    oracle.check("scenario error", false, tostring(err))
    oracle.finish()
  end
end
run = oracle.run

local function poll_until(pred, on_ready)
  local function step()
    local ok, res = pcall(pred)
    if not ok then oracle.check("sync error", false, tostring(res)) return oracle.finish() end
    if res then return oracle.run(on_ready) end
    reaper.defer(step)
  end
  reaper.defer(step)
end

function oracle.start(mode, body)
  if mode == "device" then
    local path = os.getenv("SPEED_REAPER_TRACE")
    return poll_until(function()
      local f = path and io.open(path, "r")
      if not f then return false end
      local data = f:read("*a")
      f:close()
      return data:find("log connected", 1, true) ~= nil
    end, body)
  end
  local last, quiet = reaper.time_precise(), 0
  poll_until(function()
    local now = reaper.time_precise()
    quiet = (now - last < 0.1) and quiet + 1 or 0
    last = now
    return quiet >= 10
  end, body)
end
start = oracle.start

function oracle.read_trace()
  local path = os.getenv("SPEED_REAPER_TRACE")
  local f = path and io.open(path, "r")
  oracle.check("trace readable", f ~= nil, tostring(path))
  if not f then return nil end
  local lines = {}
  for line in f:lines() do lines[#lines + 1] = line end
  f:close()
  return lines
end

function oracle.find_line(lines, needle, from)
  for i = from or 1, #lines do
    if lines[i]:find(needle, 1, true) then return i end
  end
  return 0
end

function oracle.window(samples, tmin, tmax)
  local a, b
  for i = 1, #samples do
    local t = samples[i].t
    if t >= tmin and t <= tmax then a = a or i; b = i end
  end
  return a, b
end
read_trace, find_line, window = oracle.read_trace, oracle.find_line, oracle.window

return oracle
