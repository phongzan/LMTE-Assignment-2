from pathlib import Path

from docx import Document


SOURCE = Path(r"C:\Users\Lenovo\Downloads\Assignment_2_final (1).docx")
OUTPUT = Path(r"C:\Users\Lenovo\Desktop\LMTE assg2\Assignment_2_final_roles_revised.docx")


def replace_paragraph_text(paragraph, old, new):
    if old not in paragraph.text:
        return False
    replaced = paragraph.text.replace(old, new)
    if paragraph.runs:
        paragraph.runs[0].text = replaced
        for run in paragraph.runs[1:]:
            run.text = ""
    else:
        paragraph.add_run(replaced)
    return True


doc = Document(SOURCE)

# Formal name on the cover.
doc.tables[0].cell(2, 0).text = "Dongze Yu(Bruce)"

# Roles paragraph: preserve the cover-page allocation as the source of truth.
old_roles = (
    "The group has two members. Phong designed the syntax and semantics and owns the front end, "
    "which turns source text into an AST. [Partner name] designed the compiler structure and owns "
    "the back end, which checks and runs the AST. The split follows the pipeline, so each member "
    "owns a complete part and the two parts meet at shared interfaces (T2) agreed before coding starts."
)
new_roles = (
    "The group has two members. Phong designed the syntax and semantics and owns the front end, "
    "which turns source text into an AST. Dongze Yu(Bruce) designed the compiler structure and owns "
    "the back end, which checks and runs the AST. The split follows the pipeline, so each member "
    "owns a complete part and the two parts meet at shared interfaces (T2) agreed before coding starts."
)
assert any(replace_paragraph_text(p, old_roles, new_roles) for p in doc.paragraphs)

# Table 3: allocation of tasks and report sections.
allocation = doc.tables[3]
allocation.cell(1, 0).text = "Phong"
allocation.cell(1, 1).text = (
    "T1 Language specification and grammar\n"
    "T3 Lexer\n"
    "T4 Parser and AST\n"
    "T5 Syntax error recovery\n"
    "T9 Front-end tests (U1-U6, E1-E8)"
)
allocation.cell(1, 2).text = (
    "Language Features and Idiosyncrasies\n"
    "Syntax and Semantics\n"
    "Grammar"
)
allocation.cell(1, 3).text = "T6, T7, T8\nCompiler Structure"

allocation.cell(2, 0).text = "Dongze Yu(Bruce)"
allocation.cell(2, 1).text = (
    "T6 Semantic analysis\n"
    "T7 Interpreter core\n"
    "T8 Runtime: scope, functions, history, arrays, I/O, memory\n"
    "T9 Runtime tests (U7, E9-E25)"
)
allocation.cell(2, 2).text = "Compiler Structure"
allocation.cell(2, 3).text = "T3, T4, T5\nSyntax and Semantics"

allocation.cell(3, 0).text = "Both"
allocation.cell(3, 1).text = (
    "T2 Shared interfaces\n"
    "T9 System tests P1-P23 and test runner\n"
    "T10 Integration, user guide and final report"
)

# Appendix F workflow: swap the implementation work to match the cover roles.
workflow = doc.tables[11]
workflow.cell(0, 1).text = "Dongze Yu(Bruce)"
workflow.cell(0, 2).text = "Phong"
workflow.cell(1, 1).text = "T1 with Phong"
workflow.cell(1, 2).text = "T1 with Dongze Yu(Bruce)"
workflow.cell(2, 1).text = "T2 with Phong"
workflow.cell(2, 2).text = "T2 with Dongze Yu(Bruce)"
workflow.cell(3, 1).text = "T7 Interpreter core using hand-built ASTs"
workflow.cell(3, 2).text = "T3 Lexer; T4 Parser and AST"
workflow.cell(4, 1).text = "T6 Semantic analysis; T8 scope, functions, history, arrays and I/O"
workflow.cell(4, 2).text = "T5 Syntax error reporting and recovery"
workflow.cell(5, 1).text = "T9 tests with Phong"
workflow.cell(5, 2).text = "T9 tests with Dongze Yu(Bruce)"
workflow.cell(6, 1).text = "T10 CI, release checks and report review"
workflow.cell(6, 2).text = "T10 review, guide and report"

# Appendix H Gantt owners: front end belongs to Phong; back end belongs to Bruce.
gantt = doc.tables[12]
owners = {
    "T1": "Joint",
    "T2": "Joint",
    "T3": "Phong",
    "T4": "Phong",
    "T5": "Phong",
    "T6": "Bruce",
    "T7": "Bruce",
    "T8": "Bruce",
    "T9": "Joint",
    "T10": "Joint",
}
for row in gantt.rows[1:]:
    task_id = row.cells[0].text.strip()
    if task_id in owners:
        row.cells[2].text = owners[task_id]

# Preserve table header rows across page breaks where supported.
for table in (allocation, workflow, gantt):
    tr_pr = table.rows[0]._tr.get_or_add_trPr()
    from docx.oxml import OxmlElement
    header = OxmlElement("w:tblHeader")
    header.set("{http://schemas.openxmlformats.org/wordprocessingml/2006/main}val", "true")
    tr_pr.append(header)

doc.save(OUTPUT)
print(OUTPUT)
