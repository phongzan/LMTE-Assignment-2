from pathlib import Path

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Cm, Pt, RGBColor


OUT = Path(r"C:\Users\Lenovo\Desktop\LMTE assg2\Compiler_Structure_Table.docx")


def set_cell_margins(cell, top=90, start=100, bottom=90, end=100):
    tc = cell._tc
    tc_pr = tc.get_or_add_tcPr()
    tc_mar = tc_pr.first_child_found_in("w:tcMar")
    if tc_mar is None:
        tc_mar = OxmlElement("w:tcMar")
        tc_pr.append(tc_mar)
    for edge, value in (("top", top), ("start", start), ("bottom", bottom), ("end", end)):
        node = tc_mar.find(qn(f"w:{edge}"))
        if node is None:
            node = OxmlElement(f"w:{edge}")
            tc_mar.append(node)
        node.set(qn("w:w"), str(value))
        node.set(qn("w:type"), "dxa")


def set_cell_shading(cell, fill):
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = tc_pr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tc_pr.append(shd)
    shd.set(qn("w:fill"), fill)


def set_table_borders(table, color="000000", size="8"):
    tbl_pr = table._tbl.tblPr
    borders = tbl_pr.find(qn("w:tblBorders"))
    if borders is None:
        borders = OxmlElement("w:tblBorders")
        tbl_pr.append(borders)
    for edge in ("top", "left", "bottom", "right", "insideH", "insideV"):
        tag = qn(f"w:{edge}")
        el = borders.find(tag)
        if el is None:
            el = OxmlElement(f"w:{edge}")
            borders.append(el)
        el.set(qn("w:val"), "single")
        el.set(qn("w:sz"), size)
        el.set(qn("w:space"), "0")
        el.set(qn("w:color"), color)


def set_repeat_table_header(row):
    tr_pr = row._tr.get_or_add_trPr()
    tbl_header = OxmlElement("w:tblHeader")
    tbl_header.set(qn("w:val"), "true")
    tr_pr.append(tbl_header)


def add_cell_text(cell, text, bold=False, italic=False, size=9.2):
    p = cell.paragraphs[0]
    p.paragraph_format.space_before = Pt(0)
    p.paragraph_format.space_after = Pt(0)
    p.paragraph_format.line_spacing = 1.02
    run = p.add_run(text)
    run.bold = bold
    run.italic = italic
    run.font.name = "Arial"
    run._element.get_or_add_rPr().rFonts.set(qn("w:ascii"), "Arial")
    run._element.get_or_add_rPr().rFonts.set(qn("w:hAnsi"), "Arial")
    run.font.size = Pt(size)
    run.font.color.rgb = RGBColor(0, 0, 0)
    cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
    set_cell_margins(cell)


doc = Document()
section = doc.sections[0]
section.page_width = Cm(21.0)
section.page_height = Cm(29.7)
section.top_margin = Cm(1.25)
section.bottom_margin = Cm(1.25)
section.left_margin = Cm(1.35)
section.right_margin = Cm(1.35)

normal = doc.styles["Normal"]
normal.font.name = "Arial"
normal._element.rPr.rFonts.set(qn("w:ascii"), "Arial")
normal._element.rPr.rFonts.set(qn("w:hAnsi"), "Arial")
normal.font.size = Pt(9.5)

title = doc.add_paragraph()
title.paragraph_format.space_after = Pt(3)
title.paragraph_format.keep_with_next = True
run = title.add_run("Compiler Structure")
run.bold = True
run.font.name = "Arial"
run._element.get_or_add_rPr().rFonts.set(qn("w:ascii"), "Arial")
run._element.get_or_add_rPr().rFonts.set(qn("w:hAnsi"), "Arial")
run.font.size = Pt(18)
run.font.color.rgb = RGBColor(0, 70, 95)

intro = doc.add_paragraph()
intro.paragraph_format.space_before = Pt(0)
intro.paragraph_format.space_after = Pt(6)
intro.paragraph_format.keep_with_next = True
intro_run = intro.add_run(
    "Source code is processed through lexical analysis, parsing, semantic analysis, "
    "and interpretation or code generation."
)
intro_run.bold = True
intro_run.font.name = "Arial"
intro_run._element.get_or_add_rPr().rFonts.set(qn("w:ascii"), "Arial")
intro_run._element.get_or_add_rPr().rFonts.set(qn("w:hAnsi"), "Arial")
intro_run.font.size = Pt(10)

headers = ["Stage", "Example / output", "Processing rule"]
rows = [
    (
        "Tokenisation / Lexical Analysis",
        "20 -> stock\n\nNUMBER(20), ARROW, IDENTIFIER(stock), NEWLINE",
        "The lexer converts source text into tokens. It recognises numbers, strings, Booleans, identifiers, keywords, operators, brackets, braces and commas. Each statement occupies one line; # begins a comment. Invalid characters and unfinished strings are lexical errors. The scanning method and source-position tracking are pending group confirmation.",
    ),
    (
        "Parsing",
        "20 -> stock\n\nAssignment(value: 20, target: stock)",
        "The parser checks grammatical structure and produces an Abstract Syntax Tree (AST). It must recognise expressions, assignments, constants, selections, three loop forms, functions, pipelines, arrays, scope operations, history operations and input/output. Multiplication, division and modulo bind more tightly than addition and subtraction. The parsing algorithm and remaining precedence rules are pending confirmation.",
    ),
    (
        "Syntactic / Semantic Analysis",
        "100 => MAX\n120 -> MAX\n\nThe second statement is rejected.",
        "Syntax checking rejects incomplete or incorrectly ordered constructs. Semantic checks require Boolean conditions, protect constants from reassignment or rewind, reject division by zero and invalid array indices, enforce local loop variables and function parameters, validate function calls and pipelines, resolve outward reads and explicit outer writes, and verify that requested history values exist.",
    ),
    (
        "Interpretation / Code Generation",
        "Source -> Tokens -> AST -> Checks -> Execution / Target Code",
        "The final stage evaluates the checked AST directly if an interpreter is selected, or translates it to a stated target if a compiler is selected. Runtime support must manage scoped values, arrays, function calls, input/output, variable histories, rewind and then pipelines. The implementation type, target platform and optimisation strategy are pending group confirmation; optimisations must not change observable history.",
    ),
]

table = doc.add_table(rows=1, cols=3)
table.alignment = WD_TABLE_ALIGNMENT.CENTER
table.autofit = False
widths = [Cm(4.0), Cm(5.2), Cm(9.1)]
for i, (cell, text) in enumerate(zip(table.rows[0].cells, headers)):
    cell.width = widths[i]
    set_cell_shading(cell, "E7E6E6")
    add_cell_text(cell, text, bold=True, size=10)
set_repeat_table_header(table.rows[0])

for stage, example, rule in rows:
    cells = table.add_row().cells
    for i, value in enumerate((stage, example, rule)):
        cells[i].width = widths[i]
        add_cell_text(cells[i], value, bold=(i == 0), size=8.9 if i == 2 else 9.1)

set_table_borders(table)

note = doc.add_paragraph()
note.paragraph_format.space_before = Pt(5)
note.paragraph_format.space_after = Pt(0)
note_run = note.add_run(
    "Pending decisions: interpreter or compiler, implementation language, lexer and parser algorithms, "
    "complete operator precedence, precise outer semantics, array updates, and the exact behaviour of history and rewind."
)
note_run.italic = True
note_run.font.name = "Arial"
note_run._element.get_or_add_rPr().rFonts.set(qn("w:ascii"), "Arial")
note_run._element.get_or_add_rPr().rFonts.set(qn("w:hAnsi"), "Arial")
note_run.font.size = Pt(8.5)

props = doc.core_properties
props.title = "Compiler Structure"
props.subject = "LMTE Assignment 2 language processing design"
props.author = "LMTE Assignment 2 Group"

doc.save(OUT)
print(OUT)
