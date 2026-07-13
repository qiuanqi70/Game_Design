function Header(el)
  if not el.classes:includes("unnumbered") then
    el.classes:insert("unnumbered")
  end
  return el
end

function Image(el)
  if el.attributes["width"] == nil then
    el.attributes["width"] = "90%"
  end
  return el
end

