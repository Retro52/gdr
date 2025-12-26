#!/usr/bin/env python3
"""
Codegen parses C++ headers and generates ImGui editor code based on comment annotations.

Usage:
    python codegen.py input.hpp -o output.hpp

Supported annotations:
    // @imgui           - Mark struct/class for ImGui generation
    // @color           - Treat field as color
    // @range(min, max) - Lower/Upper bounds for sliders, drags, etc.
    // @readonly        - Display but don't allow editing
    // @hide            - Skip this field entirely
    // @name("Display") - Custom display name
"""

import argparse
import re
import subprocess
from pathlib import Path
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from typing import Optional
from enum import Enum, auto

try:
    import tree_sitter_cpp as tscpp
    from tree_sitter import Language, Parser
except ImportError:
    print("Error: tree-sitter not found.", file=sys.stderr)
    print("Install with: pip install tree-sitter tree-sitter-cpp", file=sys.stderr)
    sys.exit(1)

try:
    from jinja2 import Environment, FileSystemLoader
except ImportError:
    print("Error: jinja2 not found.", file=sys.stderr)
    print("Install with: pip install Jinja2", file=sys.stderr)
    sys.exit(1)


class FieldAttribute(Enum):
    NONE = auto()
    COLOR = auto()
    READONLY = auto()
    HIDDEN = auto()
    DEBUG_ONLY = auto()
    NDEBUG_ONLY = auto()


@dataclass
class FieldInfo:
    name: str
    type_name: str
    attributes: set[FieldAttribute] = field(default_factory=set)
    range_min: Optional[float] = None
    range_max: Optional[float] = None
    display_name: Optional[str] = None
    line: int = 0

    @property
    def readonly(self) -> bool:
        return FieldAttribute.READONLY in self.attributes

    @property
    def hidden(self) -> bool:
        return FieldAttribute.HIDDEN in self.attributes

    @property
    def debug_only(self) -> bool:
        return FieldAttribute.DEBUG_ONLY in self.attributes

    @property
    def ndebug_only(self) -> bool:
        return FieldAttribute.NDEBUG_ONLY in self.attributes


@dataclass
class StructInfo:
    name: str
    fields: list[FieldInfo] = field(default_factory=list)
    namespace: Optional[str] = None
    display_name: Optional[str] = None
    line: int = 0

    @property
    def full_name(self) -> str:
        if self.namespace:
            return f"{self.namespace}::{self.name}"
        return self.name


@dataclass
class WidgetBinding:
    cpp_type: str
    widget: str
    color_widget: Optional[str] = None
    readonly_widget: Optional[str] = None


@dataclass
class Config:
    bindings: list[WidgetBinding] = field(default_factory=list)
    fallback_widget: str = 'false; ImGui::TextDisabled("{display}: [{type}]")'

    def get_binding(self, type_name: str) -> Optional[WidgetBinding]:
        # Normalize type name (remove const, &, *, extra spaces)
        normalized = re.sub(r'\b(const|volatile)\b', '', type_name)
        normalized = re.sub(r'[&*]', '', normalized)
        normalized = normalized.strip()

        for b in self.bindings:
            if b.cpp_type == normalized:
                return b
        # Try without namespace
        simple = normalized.split("::")[-1]
        for b in self.bindings:
            if b.cpp_type == simple:
                return b
        return None


def load_config(path: str) -> Config:
    tree = ET.parse(path)
    root = tree.getroot()
    config = Config()

    fallback_elem = root.find("fallback")
    if fallback_elem is not None and fallback_elem.text:
        config.fallback_widget = fallback_elem.text.strip()

    bindings_elem = root.find("bindings")
    if bindings_elem is not None:
        for b_elem in bindings_elem.findall("binding"):
            cpp_type = b_elem.get("type", "")
            widget_elem = b_elem.find("widget")
            color_elem = b_elem.find("color_widget")
            readonly_elem = b_elem.find("readonly_widget")

            if cpp_type and widget_elem is not None and widget_elem.text:
                config.bindings.append(WidgetBinding(
                    cpp_type=cpp_type,
                    widget=widget_elem.text.strip(),
                    color_widget=color_elem.text.strip() if color_elem is not None and color_elem.text else None,
                    readonly_widget=readonly_elem.text.strip() if readonly_elem is not None and readonly_elem.text else None,
                ))

    return config


# Regex patterns for annotations
ANNOTATION_PATTERNS = {
    'imgui': re.compile(r'@imgui\b'),
    'color': re.compile(r'@color\b'),
    'readonly': re.compile(r'@readonly\b'),
    'hide': re.compile(r'@hide\b'),
    'range': re.compile(r'@range\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)'),
    'name': re.compile(r'@name\s*\(\s*"([^"]+)"\s*\)'),
}


def parse_annotations(comment: str) -> dict:
    """Parse annotations from a comment string."""
    result = {
        'imgui': False,
        'color': False,
        'readonly': False,
        'hide': False,
        'range': None,
        'name': None,
    }

    if ANNOTATION_PATTERNS['imgui'].search(comment):
        result['imgui'] = True
    if ANNOTATION_PATTERNS['color'].search(comment):
        result['color'] = True
    if ANNOTATION_PATTERNS['readonly'].search(comment):
        result['readonly'] = True
    if ANNOTATION_PATTERNS['hide'].search(comment):
        result['hide'] = True

    range_match = ANNOTATION_PATTERNS['range'].search(comment)
    if range_match:
        try:
            result['range'] = (float(range_match.group(1)), float(range_match.group(2)))
        except ValueError:
            pass

    name_match = ANNOTATION_PATTERNS['name'].search(comment)
    if name_match:
        result['name'] = name_match.group(1)

    return result


def apply_annotations_to_field(field: FieldInfo, anns: dict):
    """Apply parsed annotations to a field."""
    if anns['color']:
        field.attributes.add(FieldAttribute.COLOR)
    if anns['readonly']:
        field.attributes.add(FieldAttribute.READONLY)
    if anns['hide']:
        field.attributes.add(FieldAttribute.HIDDEN)
    if anns['range']:
        field.range_min, field.range_max = anns['range']
    if anns['name']:
        field.display_name = anns['name']


def get_node_text(node, source: bytes) -> str:
    """Extract text from a tree-sitter node."""
    return source[node.start_byte:node.end_byte].decode('utf-8')


def find_preceding_comment(node, source: bytes, all_comments: list) -> Optional[str]:
    """Find comment immediately preceding a node (on previous line or same line before)."""
    node_line = node.start_point[0]

    # Look for comments on the line before or same line
    for comment_node in reversed(all_comments):
        comment_line = comment_node.end_point[0]
        # Comment ends on previous line or same line before the node
        if comment_line == node_line - 1 or (comment_line == node_line and comment_node.end_byte < node.start_byte):
            return get_node_text(comment_node, source)

    return None


def find_trailing_comment(node, source: bytes, all_comments: list) -> Optional[str]:
    """Find comment on the same line after a node."""
    node_line = node.end_point[0]

    for comment_node in all_comments:
        comment_line = comment_node.start_point[0]
        if comment_line == node_line and comment_node.start_byte > node.end_byte:
            return get_node_text(comment_node, source)

    return None


def collect_comments(node, source: bytes, comments: list):
    """Recursively collect all comment nodes."""
    if node.type == 'comment':
        comments.append(node)
    for child in node.children:
        collect_comments(child, source, comments)


def extract_type_name(type_node, source: bytes) -> str:
    """Extract the type name from a type node, handling templates etc."""
    return get_node_text(type_node, source).strip()


def has_const_qualifier(node, source: bytes) -> bool:
    """Check if a field declaration has const qualifier."""
    for child in node.children:
        if child.type == 'type_qualifier':
            text = get_node_text(child, source)
            if text == 'const':
                return True
    # Also check the raw text for const at the beginning
    text = get_node_text(node, source)
    return text.strip().startswith('const ')


def parse_field_declaration(node, source: bytes, comments: list) -> Optional[FieldInfo]:
    """Parse a field_declaration node."""
    type_name = None
    field_name = None
    is_const = has_const_qualifier(node, source)

    for child in node.children:
        if child.type == 'type_qualifier' and get_node_text(child, source) == 'const':
            is_const = True
        elif child.type in ('primitive_type', 'type_identifier', 'qualified_identifier',
                            'template_type', 'sized_type_specifier'):
            type_name = extract_type_name(child, source)
        elif child.type in ('field_identifier', 'field_declarator', 'identifier'):
            field_name = get_node_text(child, source)
        elif child.type == 'pointer_declarator':
            # Handle pointer types
            for sub in child.children:
                if sub.type in ('identifier', 'field_identifier'):
                    field_name = get_node_text(sub, source)
            if type_name:
                type_name += '*'

    if not type_name or not field_name:
        return None

    field = FieldInfo(
        name=field_name,
        type_name=type_name,
        line=node.start_point[0] + 1
    )

    if is_const:
        field.attributes.add(FieldAttribute.READONLY)

    preceding = find_preceding_comment(node, source, comments)
    if preceding:
        anns = parse_annotations(preceding)
        apply_annotations_to_field(field, anns)

    return field


def parse_debug_macro_field(node, source: bytes, comments: list) -> Optional[FieldInfo]:
    """
    Parse DEBUG_ONLY(type name) or NDEBUG_ONLY(type name) macro invocations.
    Tree-sitter parses these as expression_statement -> call_expression.
    """
    if node.type != 'expression_statement':
        return None

    call_expr = None
    for child in node.children:
        if child.type == 'call_expression':
            call_expr = child
            break

    if not call_expr:
        return None

    # Get function name and arguments
    func_name = None
    args = None
    for child in call_expr.children:
        if child.type == 'identifier':
            func_name = get_node_text(child, source)
        elif child.type == 'argument_list':
            args = child

    if func_name not in ('DEBUG_ONLY', 'NDEBUG_ONLY') or not args:
        return None

    # Extract the content inside parentheses
    args_text = get_node_text(args, source)
    # Remove outer parentheses
    inner = args_text[1:-1].strip()

    # Parse as "type name" - simple heuristic: last word is the name
    # Handle cases like "cpp::stack_string name" or "int x"
    parts = inner.rsplit(None, 1)
    if len(parts) != 2:
        return None

    type_name, field_name = parts
    # Remove trailing semicolon if present
    field_name = field_name.rstrip(';')

    field = FieldInfo(
        name=field_name,
        type_name=type_name,
        line=node.start_point[0] + 1
    )

    # Mark with appropriate attribute
    if func_name == 'DEBUG_ONLY':
        field.attributes.add(FieldAttribute.DEBUG_ONLY)
    else:
        field.attributes.add(FieldAttribute.NDEBUG_ONLY)

    # Check for preceding comment
    preceding = find_preceding_comment(node, source, comments)
    if preceding:
        anns = parse_annotations(preceding)
        apply_annotations_to_field(field, anns)

    # Check for trailing comment
    trailing = find_trailing_comment(node, source, comments)
    if trailing:
        anns = parse_annotations(trailing)
        apply_annotations_to_field(field, anns)

    return field


def parse_struct(node, source: bytes, comments: list, namespace: Optional[str] = None) -> Optional[StructInfo]:
    """Parse a struct_specifier or class_specifier node."""
    # Find struct name
    name = None
    body = None

    for child in node.children:
        if child.type == 'type_identifier':
            name = get_node_text(child, source)
        elif child.type == 'field_declaration_list':
            body = child

    if not name or not body:
        return None

    # Check for @imgui annotation in preceding comment
    preceding = find_preceding_comment(node, source, comments)
    if not preceding or not parse_annotations(preceding)['imgui']:
        return None

    anns = parse_annotations(preceding)

    struct = StructInfo(
        name=name,
        namespace=namespace,
        line=node.start_point[0] + 1,
        display_name=anns.get('name')
    )

    # Parse fields
    for child in body.children:
        if child.type == 'field_declaration':
            field = parse_field_declaration(child, source, comments)
            if field:
                struct.fields.append(field)
        elif child.type == 'expression_statement':
            field = parse_debug_macro_field(child, source, comments)
            if field:
                struct.fields.append(field)

    return struct


def parse_header(header_path: str) -> list[StructInfo]:
    """Parse a C++ header file and extract annotated structs."""

    CPP_LANGUAGE = Language(tscpp.language())
    parser = Parser(CPP_LANGUAGE)

    with open(header_path, 'rb') as f:
        source = f.read()

    tree = parser.parse(source)

    # Collect all comments first
    comments = []
    collect_comments(tree.root_node, source, comments)

    structs = []
    current_namespace = None

    def visit(node, namespace=None):
        nonlocal current_namespace

        if node.type == 'namespace_definition':
            # Extract namespace name
            ns_name = None
            ns_body = None
            for child in node.children:
                if child.type == 'identifier':
                    ns_name = get_node_text(child, source)
                elif child.type == 'declaration_list':
                    ns_body = child

            if ns_name and ns_body:
                full_ns = f"{namespace}::{ns_name}" if namespace else ns_name
                for child in ns_body.children:
                    visit(child, full_ns)
            return

        if node.type in ('struct_specifier', 'class_specifier'):
            struct = parse_struct(node, source, comments, namespace)
            if struct:
                structs.append(struct)

        for child in node.children:
            visit(child, namespace)

    visit(tree.root_node)
    return structs


def prettify_name(name: str) -> str:
    """Convert snake_case to Title Case."""
    return name.replace("_", " ").title()


def apply_template(template: str, **kwargs) -> str:
    """Apply template substitution."""
    result = template
    for key, value in kwargs.items():
        result = result.replace(f"{{{key}}}", str(value))
    return result


def generate_widget(field: FieldInfo, config: Config, indent: str = "    ") -> Optional[str]:
    """Generate widget code for a single field."""
    if FieldAttribute.HIDDEN in field.attributes:
        return None

    display = field.display_name or prettify_name(field.name)
    var = field.name
    type_name = field.type_name

    is_color = FieldAttribute.COLOR in field.attributes
    is_readonly = FieldAttribute.READONLY in field.attributes
    is_debug_only = FieldAttribute.DEBUG_ONLY in field.attributes
    is_ndebug_only = FieldAttribute.NDEBUG_ONLY in field.attributes

    binding = config.get_binding(type_name)

    if not binding:
        template = config.fallback_widget
    elif is_readonly and binding.readonly_widget:
        template = binding.readonly_widget
    elif is_color and binding.color_widget:
        template = binding.color_widget
    else:
        template = binding.widget

    has_range = field.range_min is not None and field.range_max is not None
    min_val = field.range_min if field.range_min is not None else 0.0
    max_val = field.range_max if field.range_max is not None else 0.0

    code = apply_template(
        template,
        display=display,
        var=var,
        type=type_name,
        min=f"{min_val}f" if has_range else "0.0f",
        max=f"{max_val}f" if has_range else "0.0f",
        min_int=int(min_val) if has_range else 0,
        max_int=int(max_val) if has_range else 0,
    )

    lines = code.split('\n')
    indented = '\n'.join(indent + line for line in lines)

    if is_debug_only:
        indented = f"{indent}DEBUG_ONLY(\n{indented}\n{indent})"
    elif is_ndebug_only:
        indented = f"{indent}NDEBUG_ONLY(\n{indented}\n{indent})"

    return indented


def run_clang_format(code: str, formatter: str) -> str:
    """Run clang-format on the generated code. Returns original if clang-format unavailable."""
    cmd = [formatter]
    try:
        result = subprocess.run(
            cmd,
            input=code,
            capture_output=True,
            text=True,
            timeout=30,
        )
        if result.returncode == 0:
            return result.stdout
        else:
            print(f"Warning: clang-format failed: {result.stderr}", file=sys.stderr)
            return code
    except subprocess.TimeoutExpired:
        print("Warning: clang-format timed out", file=sys.stderr)
        return code
    except Exception as e:
        print(f"Warning: clang-format error: {e}", file=sys.stderr)
        return code


@dataclass
class GeneratorOptions:
    namespace: str = "codegen"
    generate_type_list: bool = False
    type_list_name: str = "all_types"
    generate_names: bool = True
    param_name: str = "data"
    includes: list[str] = field(default_factory=list)


def generate_code(structs: list[StructInfo], config: Config, opts: GeneratorOptions) -> str:
    env = Environment(
        loader=FileSystemLoader("templates"),
        trim_blocks=True,
        lstrip_blocks=True,
    )

    # Register custom filters
    env.filters["prettify"] = prettify_name
    env.filters["widget_code"] = lambda fld, cfg: generate_widget(fld, cfg)

    template = env.get_template("codegen.hpp.j2")
    return template.render(structs=structs, config=config, opts=opts)


def main():
    parser = argparse.ArgumentParser(
        description="Generate ImGui editor code from C++ headers",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    parser.add_argument("input", nargs="?", help="Input C++ header file")
    parser.add_argument("-o", "--output", help="Output header file (default: stdout)")
    parser.add_argument("-c", "--config", help="Widget bindings config XML", required=True)
    parser.add_argument("-n", "--namespace", default="codegen", help="Output namespace")
    parser.add_argument("--include", action="append", default=[], dest="includes",
                        help="Add #include to generated file (can repeat)")
    parser.add_argument("--type-list", metavar="NAME", help="Generate type list with given name")
    parser.add_argument("--no-names", action="store_true", help="Don't generate type_name<T> traits")
    parser.add_argument("--param", default="data", help="Parameter name in draw functions")
    parser.add_argument("--format", action="store_true", help="Run clang-format on output")
    parser.add_argument("--formatter", default="clang-format", help="Path to the clang-format")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")

    args = parser.parse_args()

    if not args.input:
        parser.error("input file required (or use --dump-config)")

    config = load_config(args.config)

    if args.verbose:
        print(f"Parsing {args.input}...", file=sys.stderr)

    structs = parse_header(args.input)

    if not structs:
        print("Warning: No @imgui annotated structs found!", file=sys.stderr)
        return

    if args.verbose:
        print(f"Found {len(structs)} annotated structs:", file=sys.stderr)
        for s in structs:
            print(f"  - {s.full_name} ({len(s.fields)} fields)", file=sys.stderr)

    opts = GeneratorOptions(
        namespace=args.namespace,
        generate_type_list=args.type_list is not None,
        type_list_name=args.type_list or "all_types",
        generate_names=not args.no_names,
        param_name=args.param,
        includes=args.includes,
    )

    code = generate_code(structs, config, opts)

    if args.format:
        code = run_clang_format(code, args.formatter)
    if args.output:
        Path(args.output).parent.mkdir(parents=True, exist_ok=True)
        with open(args.output, "w") as f:
            f.write(code)
        print(f"Generated {args.output}: {len(structs)} types", file=sys.stderr)
    else:
        print(code)


if __name__ == "__main__":
    main()
