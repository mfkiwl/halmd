--
-- Copyright © 2010  Peter Colberg
--
-- This file is part of HALMD.
--
-- HALMD is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.
--

require("halmd.modules")

-- grab environment
local random_wrapper = {
    host = halmd_wrapper.host.random
  , read_integer = halmd_wrapper.random.random.read_integer
}
if halmd_wrapper.gpu then
    random_wrapper.gpu = halmd_wrapper.gpu.random
end
local device = require("halmd.device")
local h5 = halmd_wrapper.h5
local po = halmd_wrapper.po
local assert = assert

module("halmd.random", halmd.modules.register)

local random -- singleton instance

--
-- construct random module
--
function new(args)
    if not random then
        local file = args.file or "/dev/random" -- default value
        local seed = args.seed -- optional
        local blocks = args.blocks or 32 -- default value
        local threads = args.threads or 256 -- default value FIXME DEVICE_SCALE

        if not seed then
            seed = random_wrapper.read_integer(file)
        end
        local device = device()
        if device then
            random = random_wrapper.gpu.rand48(device, seed, blocks, threads)
        else
            random = random_wrapper.host.gfsr4(seed)
        end
    end
    return random
end

--
-- assemble module options
--
-- @param desc po.options_description
--
function options(desc)
    desc:add("seed", po.uint(), "random number generator integer seed")
    desc:add("file", po.string(), "read random seed from file")

    if random_wrapper.gpu then
        desc:add("blocks", po.uint(), "number of CUDA blocks")
        desc:add("threads", po.uint(), "number of CUDA threads per block")
    end
end

--
-- write module parameters to HDF5 group
--
-- @param random module instance
-- @param group HDF5 group
--
function write_parameters(random, group)

    -- FIXME serialize random number generator state

    if random.blocks then
        group:write_attribute("blocks", h5.uint(), random.blocks)
        group:write_attribute("threads", h5.uint(), random.threads)
    end
end
