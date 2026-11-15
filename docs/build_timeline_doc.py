from pathlib import Path
import shutil

from docx import Document
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT, WD_TABLE_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor


SOURCE = Path(r"C:\Users\Lenovo\Downloads\Assignment 2 plan.docx")
OUTPUT = Path(r"C:\Users\Lenovo\Desktop\LMTE assg2\Timeline.docx")


def set_run_font(run, size=12, bold=False, italic=False, color="000000"):
    run.font.name = "Arial"
    rpr = run._element.get_or_add_rPr()
    rpr.rFonts.set(qn("w:ascii"), "Arial")
    rpr.rFonts.set(qn("w:hAnsi"), "Arial")
    run.font.size = Pt(size)
    run.bold = bold
    run.italic = italic
    run.font.color.rgb = RGBColor.from_string(color)


def set_cell_shading(cell, fill):
    tcpr = cell._tc.get_or_add_tcPr()
    shd = tcpr.find(qn("w:shd"))
    if shd is None:
        shd = OxmlElement("w:shd")
        tcpr.append(shd)
    shd.set(qn("w:fill"), fill)
    shd.set(qn("w:val"), "clear")


def set_cell_margins(cell, top=85, left=70, bottom=85, right=70):
    tcpr = cell._tc.get_or_add_tcPr()
    tc_mar = tcpr.find(qn("w:tcMar"))
    if tc_mar is None:
        tc_mar = OxmlElement("w:tcMar")
        tcpr.append(tc_mar)
    for edge, value in (("top", top), ("left", left), ("bottom", bottom), ("right", right)):
        element = tc_mar.find(qn(f"w:{edge}"))
        if element is None:
            element = OxmlElement(f"w:{edge}")
            tc_mar.append(element)
        element.set(qn("w:w"), str(value))
        element.set(qn("w:type"), "dxa")


def set_cell_borders(cell, color="000000", size="6"):
    tcpr = cell._tc.get_or_add_tcPr()
    borders = tcpr.find(qn("w:tcBorders"))
    if borders is None:
        borders = OxmlElement("w:tcBorders")
        tcpr.append(borders)
    for edge in ("top", "left", "bottom", "right"):
        node = borders.find(qn(f"w:{edge}"))
        if node is None:
            node = OxmlElement(f"w:{edge}")
            borders.append(node)
        node.set(qn("w:val"), "single")
        node.set(qn("w:sz"), size)
        node.set(qn("w:space"), "0")
        node.set(qn("w:color"), color)


def set_repeat_header(row):
    trpr = row._tr.get_or_add_trPr()
    header = OxmlElement("w:tblHeader")
    header.set(qn("w:val"), "true")
    trpr.append(header)


def fill_cell(cell, text, size=9.4, bold=False, align=WD_ALIGN_PARAGRAPH.LEFT, color="000000"):
    cell.text = ""
    p = cell.paragraphs[0]
    p.alignment = align
    p.paragraph_format.space_before = Pt(0)
    p.paragraph_format.space_after = Pt(0)
    p.paragraph_format.line_spacing = 1.0
    run = p.add_run(text)
    set_run_font(run, size=size, bold=bold, color=color)
    cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
    set_cell_margins(cell)
    set_cell_borders(cell)


shutil.copy2(SOURCE, OUTPUT)
doc = Document(OUTPUT)

# Preserve the source package and section properties while replacing the body.
body = doc._element.body
sect_pr = body.sectPr
for child in list(body):
    if child is not sect_pr:
        body.remove(child)

section = doc.sections[0]
section.page_width = Inches(8.27)
section.page_height = Inches(11.69)
section.left_margin = Inches(1.0)
section.right_margin = Inches(1.0)
section.top_margin = Inches(1.0)
section.bottom_margin = Inches(1.0)

heading = doc.add_paragraph()
heading.paragraph_format.space_before = Pt(10)
heading.paragraph_format.space_after = Pt(3)
heading.paragraph_format.keep_with_next = True
set_run_font(heading.add_run("Timeline"), size=18, bold=True)

intro = doc.add_paragraph()
intro.paragraph_format.space_after = Pt(4)
intro.paragraph_format.line_spacing = 1.0
set_run_font(
    intro.add_run(
        "The implementation is scheduled across six relative project weeks. "
        "The sequence follows the dependencies in Table 4 while allowing semantic analysis "
        "and the interpreter core to begin with hand-built ASTs before the parser is merged."
    ),
    size=12,
)

caption = doc.add_paragraph()
caption.paragraph_format.space_before = Pt(6)
caption.paragraph_format.space_after = Pt(3)
caption.paragraph_format.keep_with_next = True
set_run_font(caption.add_run("Table 5. Six-week implementation timeline"), size=12, bold=True)

tasks = [
    ("T1", "Language specification and grammar", "Medium", [1]),
    ("T2", "Shared interfaces: Token.h, AST.h, Value.h", "Low", [1]),
    ("T3", "Lexer", "Medium", [2]),
    ("T4", "Parser and AST", "High", [2, 3]),
    ("T5", "Syntax error reporting and recovery", "Medium", [4]),
    ("T6", "Semantic analysis", "Medium", [3, 4]),
    ("T7", "Interpreter core: expressions and control flow", "High", [2, 3, 4]),
    ("T8", "Runtime: scope, functions, history, arrays, I/O", "High", [3, 4, 5]),
    ("T9", "Test suite and automated runner", "Medium", [4, 5]),
    ("T10", "Integration, user guide and final report", "Medium", [5, 6]),
]

headers = ["ID", "Task", "Difficulty", "W1", "W2", "W3", "W4", "W5", "W6"]
table = doc.add_table(rows=1, cols=len(headers))
table.alignment = WD_TABLE_ALIGNMENT.CENTER
table.autofit = False
widths = [0.38, 2.18, 0.72, 0.50, 0.50, 0.50, 0.50, 0.50, 0.50]

for idx, (cell, label) in enumerate(zip(table.rows[0].cells, headers)):
    cell.width = Inches(widths[idx])
    fill_cell(cell, label, size=9.3, bold=True, align=WD_ALIGN_PARAGRAPH.CENTER)
    set_cell_shading(cell, "F2F2F2")
set_repeat_header(table.rows[0])

for task_id, task_name, difficulty, active_weeks in tasks:
    cells = table.add_row().cells
    values = [task_id, task_name, difficulty]
    for idx, value in enumerate(values):
        cells[idx].width = Inches(widths[idx])
        fill_cell(
            cells[idx],
            value,
            size=9.2,
            align=WD_ALIGN_PARAGRAPH.CENTER if idx in (0, 2) else WD_ALIGN_PARAGRAPH.LEFT,
        )
    for week in range(1, 7):
        idx = week + 2
        cells[idx].width = Inches(widths[idx])
        if week in active_weeks:
            fill_cell(cells[idx], "", size=9.2, align=WD_ALIGN_PARAGRAPH.CENTER)
            set_cell_shading(cells[idx], "7F7F7F")
        else:
            fill_cell(cells[idx], "", size=9.2, align=WD_ALIGN_PARAGRAPH.CENTER)

legend = doc.add_paragraph()
legend.paragraph_format.space_before = Pt(4)
legend.paragraph_format.space_after = Pt(4)
legend.paragraph_format.keep_with_next = True
set_run_font(legend.add_run("Grey cells indicate scheduled work."), size=10, italic=True)

rationale = doc.add_paragraph()
rationale.paragraph_format.space_before = Pt(2)
rationale.paragraph_format.space_after = Pt(0)
rationale.paragraph_format.line_spacing = 1.0
set_run_font(
    rationale.add_run(
        "T4, T7 and T8 receive the longest allocations because they contain the most new logic. "
        "T6 and T7 can initially be tested with hand-built ASTs, reducing idle time while T4 is completed. "
        "T9 concentrates integration and regression testing in Weeks 4 and 5, and T10 closes the project "
        "with review, documentation and release preparation. If the schedule slips, the collection loop "
        "and input feature are removed first, as stated in the task breakdown."
    ),
    size=11,
)

props = doc.core_properties
props.title = "Flux Implementation Timeline"
props.subject = "LMTE Assignment 2 project timeline"
doc.save(OUTPUT)
print(OUTPUT)
