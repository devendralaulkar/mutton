/*
  Copyright (c) 2013 Matthew Stump

  This file is part of libprz.

  libprz is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation, either version 3 of the
  License, or (at your option) any later version.

  libprz is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <vector>
#include "encode.hpp"
#include "index.hpp"
#include "index_reader_writer_leveldb.hpp"

prz::index_reader_writer_leveldb_t::index_reader_writer_leveldb_t(leveldb::DB*          db,
                                                                  leveldb::ReadOptions  read_options,
                                                                  leveldb::WriteOptions write_options) :
    _db(db),
    _read_options(read_options),
    _write_options(write_options)
{

}

void
prz::index_reader_writer_leveldb_t::read_index(prz::index_partition_t partition,
                                               const prz::byte_t*     field,
                                               size_t                 field_size,
                                               prz::index_address_t   value,
                                               prz::index_t*          output)
{
    std::vector<char> start_key;
    std::vector<char> stop_key;
    encode_index_key(partition, reinterpret_cast<const char*>(field), field_size, value, 0, start_key);
    encode_index_key(partition, reinterpret_cast<const char*>(field), field_size, value, UINT64_MAX, stop_key);
    leveldb::Slice start_slice(reinterpret_cast<char*>(&start_key[0]), start_key.size());

    prz::index_t::iterator insert_iter = output->begin();

    std::auto_ptr<leveldb::Iterator> iter(_db->NewIterator(_read_options));
    for (iter->Seek(start_slice);
         iter->Valid() && memcmp(iter->key().data(), &stop_key[0], INDEX_SEGMENT_SIZE) < 0;
         iter->Next())
    {
        uint16_t temp_partition = 0;
        char*    temp_field = NULL;
        uint16_t temp_field_size = 0;
        uint64_t temp_value = 0;
        uint64_t offset = 0;
        assert(iter->value().size() == INDEX_SEGMENT_SIZE);
        prz::decode_index_key(iter->key().data(), &temp_partition, &temp_field, &temp_field_size, &temp_value, &offset);
        insert_iter = output->insert(insert_iter, new prz::index_t::index_node_t(offset, (const index_segment_ptr) iter->value().data()));
    }
}

void
prz::index_reader_writer_leveldb_t::read_segment(prz::index_partition_t partition,
                                                 const prz::byte_t*     field,
                                                 size_t                 field_size,
                                                 prz::index_address_t   value,
                                                 prz::index_address_t   offset,
                                                 prz::index_segment_ptr output)
{
    std::vector<char> key;
    leveldb::Slice key_slice(reinterpret_cast<char*>(&key[0]), key.size());
    encode_index_key(partition, reinterpret_cast<const char*>(field), field_size, value, offset, key);

    std::auto_ptr<leveldb::Iterator> iter(_db->NewIterator(_read_options));
    iter->Seek(key_slice);
    if (iter->Valid() && iter->key() == key_slice) {
        assert(iter->value().size() == INDEX_SEGMENT_SIZE);
        memcpy(output, iter->value().data(), INDEX_SEGMENT_SIZE);
    }
}

void
prz::index_reader_writer_leveldb_t::write_segment(prz::index_partition_t partition,
                                                  const prz::byte_t*     field,
                                                  size_t                 field_size,
                                                  prz::index_address_t   value,
                                                  prz::index_address_t   offset,
                                                  prz::index_segment_ptr input)
{
    std::vector<char> key;
    encode_index_key(partition, reinterpret_cast<const char*>(field), field_size, value, offset, key);
    leveldb::Status status = _db->Put(_write_options,
                                      leveldb::Slice(reinterpret_cast<char*>(&key[0]), key.size()),
                                      leveldb::Slice(reinterpret_cast<char*>(input), INDEX_SEGMENT_SIZE));
}

size_t
prz::index_reader_writer_leveldb_t::estimateSize(prz::index_partition_t partition,
                                                 const prz::byte_t*     field,
                                                 size_t                 field_size,
                                                 prz::index_address_t   value)
{
    std::vector<char> start_key;
    std::vector<char> stop_key;
    encode_index_key(partition, reinterpret_cast<const char*>(field), field_size, value, 0, start_key);
    encode_index_key(partition, reinterpret_cast<const char*>(field), field_size, value, UINT64_MAX, stop_key);
    leveldb::Range range(leveldb::Slice(reinterpret_cast<char*>(&start_key[0]), start_key.size()),
                         leveldb::Slice(reinterpret_cast<char*>(&stop_key[0]), stop_key.size()));
    prz::index_address_t size;
    _db->GetApproximateSizes(&range, 1, &size);
    return size;
}