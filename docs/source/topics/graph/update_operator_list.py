#
# Copyright 2023 Ant Group Co., Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
from secretflow_serving_lib import get_all_ops
from secretflow_serving_lib.attr_pb2 import AttrValue, AttrType
from mdutils.mdutils import MdUtils

import datetime

this_directory = os.path.abspath(os.path.dirname(__file__))

mdFile = MdUtils(
    file_name=os.path.join(this_directory, 'operator_list.md'),
)

mdFile.new_header(level=1, title='SecretFlow-Serving Operator List', style='setext')

mdFile.new_paragraph(f'Last update: {datetime.datetime.now().strftime("%c")}')

AttrTypeStrMap = {
    AttrType.UNKNOWN_AT_TYPE: 'Undefined',
    AttrType.AT_INT32: 'Integer32',
    AttrType.AT_INT64: 'Integer64',
    AttrType.AT_FLOAT: 'Float',
    AttrType.AT_DOUBLE: 'Double',
    AttrType.AT_STRING: 'String',
    AttrType.AT_BOOL: 'Boolean',
    AttrType.AT_BYTES: 'Bytes',
    AttrType.AT_INT32_LIST: 'Integer32 List',
    AttrType.AT_INT64_LIST: 'Integer64 List',
    AttrType.AT_FLOAT_LIST: 'Float List',
    AttrType.AT_DOUBLE_LIST: 'Double List',
    AttrType.AT_STRING_LIST: 'String List',
    AttrType.AT_BOOL_LIST: 'Boolean List',
    AttrType.AT_BYTES_LIST: 'Bytes List',
}


def get_atomic_attr_value(at: AttrType, attr: AttrValue):
    if at == AttrType.AT_FLOAT:
        return round(attr.f, 5)
    elif at == AttrType.AT_DOUBLE:
        return round(attr.d, 8)
    elif at == AttrType.AT_INT32:
        return attr.i32
    elif at == AttrType.AT_INT64:
        return attr.i64
    elif at == AttrType.AT_STRING:
        return attr.s
    elif at == AttrType.AT_BOOL:
        return attr.b
    elif at == AttrType.AT_FLOAT_LIST:
        return [round(f, 5) for f in attr.fs.data]
    elif at == AttrType.AT_DOUBLE_LIST:
        return [round(f, 8) for f in attr.ds.data]
    elif at == AttrType.AT_INT32_LIST:
        return list(attr.i32s.data)
    elif at == AttrType.AT_INT64_LIST:
        return list(attr.i64s.data)
    elif at == AttrType.AT_STRING_LIST:
        return list(attr.ss.data)
    elif at == AttrType.AT_BOOL_LIST:
        return list(attr.bs.data)
    elif at == AttrType.AT_BYTES_LIST:
        return list(attr.bs.data)
    else:
        return None


def parse_comp_io(md, io_defs):
    io_table_text = ['Name', 'Description']
    for io_def in io_defs:
        io_table_text.extend([io_def.name, io_def.desc])

    md.new_line()
    md.new_table(
        columns=2,
        rows=len(io_defs) + 1,
        text=io_table_text,
        text_align='left',
    )


op_list = get_all_ops()


for op in op_list:
    mdFile.new_header(
        level=2,
        title=op.name,
    )
    mdFile.new_paragraph(f'Operator version: {op.version}')
    mdFile.new_paragraph(op.desc)

    # build attrs
    if len(op.attrs):
        mdFile.new_header(
            level=3,
            title='Attrs',
        )
        attr_table_text = ["Name", "Description", "Type", "Required", "Notes"]
        for attr in op.attrs:
            name_str = attr.name
            type_str = AttrTypeStrMap[attr.type]
            notes_str = ''
            required_str = 'N/A'

            default_value = None
            if attr.is_optional:
                default_value = get_atomic_attr_value(attr.type, attr.default_value)

            if default_value is not None:
                notes_str += f'Default: {default_value}. '

            required_str = 'N' if attr.is_optional else 'Y'

            attr_table_text.extend(
                [name_str, attr.desc, type_str, required_str, notes_str.rstrip()]
            )

        mdFile.new_line()
        mdFile.new_table(
            columns=5,
            rows=len(op.attrs) + 1,
            text=attr_table_text,
            text_align='left',
        )

    # build tag
    if op.tag.returnable or op.tag.mergeable or op.tag.session_run:
        tag_rows = 0
        mdFile.new_header(
            level=3,
            title='Tags',
        )
        tag_table_text = ["Name", "Description"]
        if op.tag.returnable:
            tag_rows += 1
            tag_table_text.extend(
                ["returnable", "The operator's output can be the final result"]
            )
        if op.tag.mergeable:
            tag_rows += 1
            tag_table_text.extend(
                [
                    "mergeable",
                    "The operator accept the output of operators with different participants and will somehow merge them.",
                ]
            )
        if op.tag.session_run:
            tag_rows += 1
            tag_table_text.extend(
                ["session_run", "The operator needs to be executed in session."]
            )

        mdFile.new_line()
        mdFile.new_table(
            columns=2,
            rows=tag_rows + 1,
            text=tag_table_text,
            text_align='left',
        )

    # build inputs/output
    if len(op.inputs):
        mdFile.new_header(
            level=3,
            title='Inputs',
        )
        parse_comp_io(mdFile, op.inputs)

    mdFile.new_header(
        level=3,
        title='Output',
    )
    parse_comp_io(mdFile, [op.output])


mdFile.create_md_file()
