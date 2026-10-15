-- Documented probe: intentionally NOT in the tools/reaper-oracle REAPER_SCENARIOS suite (42-release-prep §5). Keep for manual runs: --scenario NAME.
-- oracle/scenarios/led_reads.lua — M8 research (vibes/34-m8-led-bar.md task 1).
-- No fake device: drive REAPER state via Main_OnCommand and read it back with
-- GetToggleCommandState (0=off, 1=on, -1=NA) to prove the LED-bar state reads
-- (design §5). Each check asserts that the read tracks the change made and
-- records every candidate id's value as detail (1157, 40310, 40311, 41117,
-- plus the toggle ripple actions 41990 per-track / 41991 all / 1162 on/off).
--
-- Measured (REAPER 7.81, fresh profile): the design §5 candidate ids ALL work,
-- including the "Set ripple editing ..." commands 40310/40311, which
-- GetToggleCommandState reports as 1 when their mode is active and 0
-- otherwise. The dedicated "Toggle ripple editing ..." actions 41990/41991
-- track identically. See docs/notes/m8-state-reads.md.
dofile(os.getenv("ORACLE_LIB"))

local ids = { 1157, 40310, 40311, 41117, 41990, 41991, 1162 }
local function read_states()
  local parts = {}
  for _, id in ipairs(ids) do
    table.insert(parts, id .. "=" .. tostring(reaper.GetToggleCommandState(id)))
  end
  return table.concat(parts, " ")
end
local function expect(label, cond, detail)
  check(label, cond, label .. ": " .. detail)
end

start("settled", function()
  record("app_version", reaper.GetAppVersion())
  check("baseline (fresh profile)", true, read_states())

  -- ripple off
  reaper.Main_OnCommand(40309, 0)
  reaper.defer(function()
    local s, r10, r11 = read_states(),
        reaper.GetToggleCommandState(40310), reaper.GetToggleCommandState(40311)
    expect("ripple off (40309): 40310=0, 40311=0", r10 == 0 and r11 == 0, s)

    -- ripple per-track
    reaper.Main_OnCommand(40310, 0)
    reaper.defer(function()
      local s, r10, r11 = read_states(),
          reaper.GetToggleCommandState(40310), reaper.GetToggleCommandState(40311)
      expect("ripple per-track (40310): 40310=1, 40311=0",
             r10 == 1 and r11 == 0, s)

      -- ripple all
      reaper.Main_OnCommand(40311, 0)
      reaper.defer(function()
        local s, r10, r11 = read_states(),
            reaper.GetToggleCommandState(40310), reaper.GetToggleCommandState(40311)
        expect("ripple all (40311): 40310=0, 40311=1",
               r10 == 0 and r11 == 1, s)

        -- trim-behind: find current state, then toggle 41117
        local before = reaper.GetToggleCommandState(41117)
        reaper.Main_OnCommand(41117, 0)
        reaper.defer(function()
          local after = reaper.GetToggleCommandState(41117)
          local s = read_states()
          expect("trim-behind toggled (41117) tracks",
                 before == 0 or before == 1,
                 "41117 before=" .. tostring(before) ..
                 " after=" .. tostring(after) .. " | " .. s)
          expect("trim-behind (41117): after = 1 - before",
                 after == 1 - before,
                 "41117 before=" .. tostring(before) ..
                 " after=" .. tostring(after) .. " | " .. s)

          -- snap: same dance on 1157
          local sbefore = reaper.GetToggleCommandState(1157)
          reaper.Main_OnCommand(1157, 0)
          reaper.defer(function()
            local safter = reaper.GetToggleCommandState(1157)
            local s = read_states()
            expect("snap (1157): before is a toggle state",
                   sbefore == 0 or sbefore == 1,
                   "1157 before=" .. tostring(sbefore) ..
                   " after=" .. tostring(safter) .. " | " .. s)
            expect("snap (1157) tracks: after = 1 - before",
                   safter == 1 - sbefore,
                   "1157 before=" .. tostring(sbefore) ..
                   " after=" .. tostring(safter) .. " | " .. s)
            finish()
          end)
        end)
      end)
    end)
  end)
end)
