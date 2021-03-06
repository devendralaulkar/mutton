/*
  Copyright (c) 2013 Matthew Stump

  This file is part of libmutton.

  libmutton is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  libmutton is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "range.hpp"
#include "index.hpp"

mtn::index_t::index_t(mtn_index_partition_t           partition,
                      const std::vector<mtn::byte_t>& bucket,
                      const std::vector<mtn::byte_t>& field) :
    _partition(partition),
    _bucket(bucket),
    _field(field)
{}

mtn::index_t::index_t(mtn_index_partition_t partition,
                      const mtn::byte_t*    bucket,
                      size_t                bucket_size,
                      const mtn::byte_t*    field,
                      size_t                field_size) :
    _partition(partition),
    _bucket(bucket, bucket + bucket_size),
    _field(field, field + field_size)
{}

mtn::status_t
mtn::index_t::slice(mtn::range_t*             ranges,
                    size_t                    range_count,
                    mtn::index_operation_enum operation,
                    mtn::index_slice_t&       output)
{
    mtn::status_t status;
    bool first_iteration = true;

    for (size_t r = 0; r < range_count; ++r) {

        mtn::index_t::iterator iter = _index.lower_bound(ranges[r].start);
        if (first_iteration && iter != end() && iter->first < ranges[r].limit) {
            output = *(iter->second);
            first_iteration = false;
            ++iter;
        }

        for (; iter != end() && iter->first < ranges[r].limit; ++iter) {
            status = output.execute(operation, *(iter->second), output, output);
            if (!status) {
                return status;
            }
        }
    }

    return status;
}

mtn::status_t
mtn::index_t::slice(mtn::range_t*       ranges,
                    size_t              range_count,
                    mtn::index_slice_t& output)
{
    return slice(ranges, range_count, MTN_INDEX_OP_UNION, output);
}

mtn::status_t
mtn::index_t::slice(mtn::index_slice_t& output)
{
    mtn::status_t status;

    for (mtn::index_t::iterator iter = _index.begin(); iter != _index.end(); ++iter) {
        status = output.execute(mtn::MTN_INDEX_OP_UNION, *(iter->second), output, output);
        if (!status) {
            return status;
        }
    }

    return status;
}

mtn::status_t
mtn::index_t::index_value(mtn::index_reader_writer_t& rw,
                          mtn_index_address_t         value,
                          mtn_index_address_t         who_or_what,
                          bool                        state)
{
    mtn::index_t::iterator iter = iter = _index.find(value);
    if (iter == _index.end()) {
        iter = insert(value, new mtn::index_slice_t(_partition, _bucket, _field, value)).first;
    }

    iter->second->bit(rw, who_or_what, state);
    return mtn::status_t(); // XXX TODO better error handling
}

mtn::status_t
mtn::index_t::indexed_value(mtn::index_reader_writer_t&,
                            mtn_index_address_t value,
                            mtn_index_address_t who_or_what,
                            bool*               state)
{
    mtn::index_t::iterator iter = _index.find(value);
    if (iter == _index.end()) {
        *state = false;
    }
    else {
        *state = iter->second->bit(who_or_what);
    }
    return mtn::status_t(); // XXX TODO better error handling
}

mtn::status_t
mtn::index_t::indexed_value(mtn::index_reader_writer_t&,
                            mtn_index_address_t  value,
                            mtn::index_slice_t** who_or_what)
{
    mtn::index_t::iterator iter = _index.find(value);
    if (iter == _index.end()) {
        *who_or_what = NULL;
    }
    else {
        *who_or_what = iter->second;
    }
    return mtn::status_t(); // XXX TODO better error handling
}

mtn_index_partition_t
mtn::index_t::partition() const
{
    return _partition;
}
