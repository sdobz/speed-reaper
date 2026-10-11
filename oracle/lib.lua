-- oracle/lib.lua — shared helpers for oracle scenarios (design §4).
-- Scenarios start with: dofile(os.getenv("ORACLE_LIB"))
--
-- API:
--   check(name, cond, detail)   record one pass/fail check
--   record(key, value)          attach a key/value fact (e.g. app version)
--   wait_ms(ms, fn, on_done)    defer loop: fn() each cycle until ms elapsed,
--                               then on_done() if given
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
  local t0 = reaper.get_time()
  local function step()
    if fn then fn() end
    if (reaper.get_time() - t0) * 1000 >= ms then
      if on_done then on_done() end
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
    reaper.show("oracle: ORACLE_OUT env not set")
    return
  end
  local f = io.open(out_dir .. "/probe.json", "w")
  if not f then
    reaper.show("oracle: cannot write " .. out_dir .. "/probe.json")
    return
  end
  f:write(encode({ checks = checks, info = info }) .. "\n")
  f:close()
  reaper.Main_OnCommand(40004, 0) -- Quit REAPER
end

-- Also expose the names unqualified so scenarios can just call check/record/finish.
check = oracle.check
record = oracle.record
wait_ms = oracle.wait_ms
finish = oracle.finish
return oracle
