/**
 * Copyright (c) 2011-2013 by Andrew Mustun. All rights reserved.
 * 
 * This file is part of the QCAD project.
 *
 * QCAD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * QCAD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QCAD.
 */
#include <QFont>
#include <QFontMetricsF>
#include <QTextBlock>
#include <QTextDocument>

#include "RColor.h"
#include "RDxfServices.h"
#include "RFont.h"
#include "RFontList.h"
#include "RTextRenderer.h"
#include "RPainterPathDevice.h"
#include "RPointEntity.h"
#include "RPolyline.h"
#include "RSettings.h"

QChar RTextRenderer::chDegree = QChar(0x00b0);
QChar RTextRenderer::chPlusMinus = QChar(0x00b1);
//QChar RTextRenderer::chDiameter = QChar(0x2300);
QChar RTextRenderer::chDiameter = QChar(0x00f8);

QString RTextRenderer::rxLineFeed = "\\\\p(?:x?i(\\d*\\.?\\d+);)?";
QString RTextRenderer::rxParagraphFeed = "\\\\P";
QString RTextRenderer::rxHeightChange = "\\\\H(\\d*\\.?\\d+)(x?);";
//QString RTextRenderer::rxRelativeHeightChange = "";
QString RTextRenderer::rxStackedText = "\\\\S([^^]*)\\^([^;]*);";
QString RTextRenderer::rxColorChangeIndex = "\\\\C(\\d+);";
QString RTextRenderer::rxColorChangeCustom ="\\\\c(\\d+);";
QString RTextRenderer::rxNonBreakingSpace = "\\\\~";
// TODO: break at regular spaces for line breaking:
//QString RTextRenderer::rxSpace = " ";
QString RTextRenderer::rxOverlineOn = "\\\\O";
QString RTextRenderer::rxOverlineOff = "\\\\o";
QString RTextRenderer::rxUnderlineOn = "\\\\L";
QString RTextRenderer::rxUnderlineOff = "\\\\l";
QString RTextRenderer::rxWidthChange = "\\\\W(\\d*\\.?\\d+);";
QString RTextRenderer::rxObliqueAngleChange = "\\\\Q(\\d*\\.?\\d+);";
QString RTextRenderer::rxTrackChange = "\\\\T(\\d*\\.?\\d+);";
QString RTextRenderer::rxAlignmentChange = "\\\\A(\\d+);";
QString RTextRenderer::rxFontChangeCad = "(?:\\\\F([^|]*)\\|c(\\d+);|\\\\F([^|;]*);)";
QString RTextRenderer::rxFontChangeTtf = "\\\\f([^|]*)\\|b(\\d+)\\|i(\\d+)\\|c(\\d+)\\|p(\\d+);";
QString RTextRenderer::rxBeginBlock = "\\{";
QString RTextRenderer::rxEndBlock = "\\}";
QString RTextRenderer::rxBackslash = "\\\\\\\\";
QString RTextRenderer::rxCurlyOpen = "\\\\\\{";
QString RTextRenderer::rxCurlyClose = "\\\\\\}";
QString RTextRenderer::rxDegree = "%%[dD]";
QString RTextRenderer::escDegree = "%%d";
QString RTextRenderer::rxPlusMinus = "%%[pP]";
QString RTextRenderer::escPlusMinus = "%%p";
QString RTextRenderer::rxDiameter = "%%[cC]";
QString RTextRenderer::escDiameter = "%%c";
QString RTextRenderer::rxUnicode = "\\\\[Uu]\\+([0-9a-fA-F]{4})";

QString RTextRenderer::rxAll = "("
    + RTextRenderer::rxLineFeed + "|"
    + RTextRenderer::rxParagraphFeed + "|"
    + RTextRenderer::rxHeightChange + "|"
    + RTextRenderer::rxStackedText + "|"
    + RTextRenderer::rxColorChangeIndex + "|"
    + RTextRenderer::rxColorChangeCustom + "|"
    + RTextRenderer::rxNonBreakingSpace + "|"
    + RTextRenderer::rxOverlineOn + "|"
    + RTextRenderer::rxOverlineOff + "|"
    + RTextRenderer::rxUnderlineOn + "|"
    + RTextRenderer::rxUnderlineOff + "|"
    + RTextRenderer::rxWidthChange + "|"
    + RTextRenderer::rxObliqueAngleChange + "|"
    + RTextRenderer::rxTrackChange + "|"
    + RTextRenderer::rxAlignmentChange + "|"
    + RTextRenderer::rxFontChangeCad + "|"
    + RTextRenderer::rxFontChangeTtf + "|"
    + RTextRenderer::rxBeginBlock + "|"
    + RTextRenderer::rxEndBlock + "|"
    + RTextRenderer::rxBackslash + "|"
    + RTextRenderer::rxCurlyOpen + "|"
    + RTextRenderer::rxCurlyClose + "|"
    + RTextRenderer::rxDegree + "|"
    + RTextRenderer::rxPlusMinus + "|"
    + RTextRenderer::rxDiameter + "|"
    + RTextRenderer::rxUnicode
    + ")";



RTextRenderer::RTextRenderer(const RTextBasedData& textData, bool draft, Target target, double fontHeightFactor)
    : textData(textData), target(target), height(0.0), width(0.0),
      draft(draft), fontHeightFactor(fontHeightFactor) {

    if (textData.isSimple()) {
        renderSimple();
    }
    else {
        render();
    }
}


/**
 * Renders the text data into painter paths.
 */
void RTextRenderer::renderSimple() {
    boundingBox = RBox();
    painterPaths.clear();
    richText = "";

    RVector pos = textData.getAlignmentPoint();
    if (textData.getHAlign()==RS::HAlignFit) {
        pos = textData.getPosition();
    }
    QString text = textData.getRenderedText();
    double textHeight = textData.getTextHeight();
    RS::VAlign verticalAlignment = textData.getVAlign();
    RS::HAlign horizontalAlignment = textData.getHAlign();
    QString fontName = textData.getFontName();
    bool bold = textData.isBold();
    bool italic = textData.isItalic();
    double angle = textData.getAngle();

    // degree:
    text.replace(QRegExp(RTextRenderer::rxDegree), RTextRenderer::chDegree);
    // plus minus:
    text.replace(QRegExp(RTextRenderer::rxPlusMinus), RTextRenderer::chPlusMinus);
    // diameter:
    text.replace(QRegExp(RTextRenderer::rxDiameter), RTextRenderer::chDiameter);
    // unicode:
    text = RDxfServices::parseUnicode(text);
//    QRegExp reg;
//    reg.setPattern(rxUnicode);
//    int ucPos = 0;
//    bool ok = true;
//    int uc = 0;
//    while ((ucPos = reg.indexIn(text, 0)) != -1) {
//        uc = reg.cap(1).toInt(&ok, 16);
//        if (!ok) {
//            break;
//        }
//        text.replace(ucPos, reg.matchedLength(), QChar(uc));
//    }

    bool leadingSpaces = false;
    bool trailingSpaces = false;

    if (!text.isEmpty()) {
        if (text.at(0).isSpace()) {
            leadingSpaces = true;
        }
        if (text.at(text.length()-1).isSpace()) {
            trailingSpaces = true;
        }
    }

    // implicit top level format block:
    QTextCharFormat f;
    f.setForeground(QBrush(QColor()));
    currentFormat.push(f);
    QTextLayout::FormatRange fr;
    fr.start = 0;
    fr.length = text.length();
    fr.format = currentFormat.top();
    QList<QTextLayout::FormatRange> formats;
    formats.append(fr);
    blockHeight.push(textHeight);
    blockFont.push(fontName);
    blockBold.push(bold);
    blockItalic.push(italic);
    useCadFont.push(RFontList::isCadFont(blockFont.top()));
    openTags.push(QStringList());

    double horizontalAdvance = 0.0;
    double horizontalAdvanceNoSpacing = 0.0;
    double ascent = 0.0;
    double descent = 0.0;

    // get painter paths for text at height 1.0:
    painterPaths = getPainterPathsForBlock(
                text, formats,
                horizontalAdvance,
                horizontalAdvanceNoSpacing,
                ascent, descent);

    width = horizontalAdvanceNoSpacing * textHeight;

    // transform to scale text from 1.0 to current text height:
    QTransform sizeTransform;
    sizeTransform.scale(textHeight, textHeight);

    // transform paths of text:
    boundingBox = RBox();
    for (int i=0; i<painterPaths.size(); ++i) {
        painterPaths[i].transform(sizeTransform);
        boundingBox.growToInclude(painterPaths[i].getBoundingBox());
    }

    // feature size of a text is its height
    // determines if text is displayed or only bounding box
    double featureSize = boundingBox.getHeight();

    QPen pen;
    for (int i=0; i<painterPaths.size(); ++i) {
        if (i==0) {
            pen = painterPaths[i].getPen();
            if (pen.style()==Qt::NoPen) {
                pen = QPen(painterPaths[i].getBrush().color());
            }
        }
        painterPaths[i].setFeatureSize(featureSize);
    }

    RPainterPath bbPath;
    bbPath.addBox(boundingBox);
    bbPath.setFeatureSize(-featureSize);
    bbPath.setPen(pen);
    painterPaths.append(bbPath);

    //height = boundingBox.getHeight();

    double topLine = textHeight;
    double baseLine = 0.0;
    double bottomLine = descent * textHeight;

    // at this point, the text is at 0/0 with the base line of the
    // first text line at y==0

    double xOffset = 0.0;
    double yOffset = 0.0;

    switch (verticalAlignment) {
    case RS::VAlignTop:
        yOffset = -topLine;
        break;
    case RS::VAlignMiddle:
        yOffset = -textHeight/2.0;
        break;
    case RS::VAlignBase:
        yOffset = -baseLine;
        break;
    case RS::VAlignBottom:
        yOffset = -bottomLine;
        break;
    }

    switch (horizontalAlignment) {
    default:
    case RS::HAlignAlign:
    case RS::HAlignFit:
    case RS::HAlignLeft:
        if (!leadingSpaces) {
            // move completely to the left (left border is 0.0):
            xOffset = -boundingBox.getMinimum().x;
        }
        else {
            xOffset = 0.0;
        }
        break;
    case RS::HAlignMid:
    case RS::HAlignCenter:
        if (!leadingSpaces && !trailingSpaces) {
            xOffset =  -(boundingBox.getMinimum().x +
                          boundingBox.getMaximum().x)/2.0;
        }
        else {
            xOffset = -horizontalAdvance/2.0*textHeight;
        }
        break;
    case RS::HAlignRight:
        if (!trailingSpaces) {
            xOffset = -boundingBox.getMaximum().x;
        }
        else {
            xOffset = -horizontalAdvance*textHeight;
        }
        break;
    }

    height = boundingBox.getHeight();

    QTransform globalTransform;
    globalTransform.translate(pos.x, pos.y);
    globalTransform.rotate(RMath::rad2deg(angle));
    globalTransform.translate(xOffset, yOffset);

    // apply global transform for position, angle and vertical alignment:
    boundingBox = RBox();
    for (int i=0; i<painterPaths.size(); ++i) {
        painterPaths[i].transform(globalTransform);
        boundingBox.growToInclude(painterPaths[i].getBoundingBox());
    }
}

/**
 * Renders the text data into painter paths.
 */
void RTextRenderer::render() {
    boundingBox = RBox();
    painterPaths.clear();
    richText = "";

    QString text = textData.getRenderedText();
    //RVector position = textData.getPosition();
    RVector position = textData.getAlignmentPoint();
    double textHeight = textData.getTextHeight();
    RS::VAlign verticalAlignment = textData.getVAlign();
    RS::HAlign horizontalAlignment = textData.getHAlign();
    double lineSpacingFactor = textData.getLineSpacingFactor();
    QString fontName = textData.getFontName();
    bool bold = textData.isBold();
    bool italic = textData.isItalic();
    double angle = textData.getAngle();
    //RColor color = textData.getColor(true);

    // split up text where separation is required due to line feed,
    // or any type of height change (\P, \H[0-9]*.?[0-9]+;, \S*/*;)

    // split up text at every formatting change:
    QRegExp regExpAll(rxAll);
    QStringList literals = text.split(regExpAll);

    // collect formatting change information for every literal:
    QStringList formattings;
    int pos = 0;
    while ((pos = regExpAll.indexIn(text, pos)) != -1) {
        formattings << regExpAll.cap(1);
        pos += regExpAll.matchedLength();
    }

    // x position in real units:
    double xCursor = 0.0;
    // y position for next text line:
    double yCursor = 0.0;
    // painter paths for current text line:
    QList<RPainterPath> linePaths;
    // max ascent of current text line:
    double maxAscent = 0.0;
    // max descent of current line:
    double minDescent = 0.0;
    // max descent of _previous_ line:
    double minDescentPrevious = 0.0;
    // true for the first block of the current line:
    bool firstBlockInLine = true;
    // current line has leading spaces:
    bool leadingSpaces = false;
    // current line has trailing spaces:
    bool trailingSpaces = false;
    // text has leading empty lines:
    bool leadingEmptyLines = false;
    // text has trailing empty lines:
    bool trailingEmptyLines = false;
    int lineCounter = 0;
    QString textBlock;
    QList<QTextLayout::FormatRange> formats;

    bool blockChangedHeightOrFont = false;

    // implicit top level format block:
    QTextCharFormat f;
    f.setForeground(QBrush(QColor()));
    currentFormat.push(f);
    QTextLayout::FormatRange fr;
    fr.start = 0;
    fr.length = 0;
    fr.format = currentFormat.top();
    formats.append(fr);
    blockHeight.push(textHeight);
    blockFont.push(fontName);
    blockBold.push(bold);
    blockItalic.push(italic);
    useCadFont.push(RFontList::isCadFont(fontName));
    openTags.push(QStringList());

    width = 0.0;
    height = 0.0;

    // iterate through all text blocks:
    for (int i=0; i<literals.size(); ++i) {

        // the literal text, e.g. "ABC":
        QString literal = literals.at(i);

        // the formatting _after_ the text, e.g. "\C1;"
        QString formatting;
        if (i<formattings.size()) {
            formatting = formattings.at(i);
        }

//        qDebug() << "block: " << i;
//        qDebug() << "  literal: " << literal;
//        qDebug() << "  literal length: " << literal.length();
//        qDebug() << "  formatting: " << formatting;
//        qDebug() << "  font: " << blockFont.top();
//        qDebug() << "  height: " << blockHeight.top();
//        qDebug() << "  cad font: " << useCadFont.top();
//        qDebug() << "  xCursor: " << xCursor;

        // handle literal:
        textBlock.append(literal);

        if (firstBlockInLine) {
            if (!literal.isEmpty()) {
                if (literal.at(0).isSpace()) {
                    leadingSpaces = true;
                }
            }
            else {
                if (formatting=="\\~") {
                    leadingSpaces = true;
                }
            }
            firstBlockInLine = false;
        }

        bool lineFeed = false;
        bool paragraphFeed = false;
        bool heightChange = false;
        bool stackedText = false;
        bool fontChange = false;
        bool colorChange = false;
        bool blockEnd = false;

        // detect formatting that ends the current text block:
        if (!formatting.isEmpty()) {
            fontChange = QRegExp(rxFontChangeTtf).exactMatch(formatting) ||
                QRegExp(rxFontChangeCad).exactMatch(formatting);
            colorChange = QRegExp(rxColorChangeCustom).exactMatch(formatting) ||
                QRegExp(rxColorChangeIndex).exactMatch(formatting);
            heightChange = QRegExp(rxHeightChange).exactMatch(formatting);
            stackedText = QRegExp(rxStackedText).exactMatch(formatting);
            lineFeed = QRegExp(rxLineFeed).exactMatch(formatting);
            paragraphFeed = QRegExp(rxParagraphFeed).exactMatch(formatting);
            blockEnd = QRegExp(rxEndBlock).exactMatch(formatting);
        }

        bool start = (i==0);
        bool end = (i==literals.size()-1);

        // first line is empty:
        if (textBlock.trimmed().isEmpty() && (lineFeed || paragraphFeed || end)) {
            if (start) {
                leadingEmptyLines = true;
            }
            else if (end) {
                trailingEmptyLines = true;
            }
        }

        // reached a new text block that needs to be rendered separately
        // due to line feed, height change, font change, ...:
        if (target==RichText ||
            lineFeed || paragraphFeed || heightChange || stackedText ||
            fontChange || colorChange || end
            || (blockEnd && blockChangedHeightOrFont)) {

            // render block _before_ format change that requires new block:
            if (textBlock!="") {
                // fix format lengths to match up. each format stops where
                // the next one starts. formatting that is carried over is set
                // again for the new format.
                for (int i=0; i<formats.size(); i++) {
                    int nextStart;
                    if (i<formats.size()-1) {
                        nextStart = formats[i+1].start;
                    }
                    else {
                        nextStart = textBlock.length();
                    }
                    formats[i].length = nextStart - formats[i].start;
                }

                if (target==RichText) {
                    richText += getRichTextForBlock(textBlock, formats);
                }

                if (target==PainterPaths) {
                    double horizontalAdvance = 0.0;
                    double horizontalAdvanceNoSpacing = 0.0;
                    double ascent = 0.0;
                    double descent = 0.0;
                    QList<RPainterPath> paths;

                    // get painter paths for current text block at height 1.0:
                    paths = getPainterPathsForBlock(
                                textBlock, formats,
                                horizontalAdvance,
                                horizontalAdvanceNoSpacing,
                                ascent, descent);

                    // transform to scale text from 1.0 to current text height:
                    QTransform sizeTransform;
                    sizeTransform.scale(blockHeight.top(), blockHeight.top());

                    // transform for current block due to xCursor position:
                    QTransform blockTransform;
                    blockTransform.translate(xCursor, 0);

                    // combine transforms for current text block:
                    QTransform allTransforms = sizeTransform;
                    allTransforms *= blockTransform;

                    maxAscent = qMax(maxAscent, ascent * blockHeight.top());
                    minDescent = qMin(minDescent, descent * blockHeight.top());

                    // transform paths of current block and append to paths
                    // of current text line:
                    for (int i=0; i<paths.size(); ++i) {
                        RPainterPath p = paths.at(i);
                        p.transform(allTransforms);
                        linePaths.append(p);
                    }

                    xCursor += horizontalAdvance * blockHeight.top();
                }
            }

            // empty text, might be line feed, we need ascent, descent anyway:
            else if (lineFeed || paragraphFeed || end) {
                if (target==PainterPaths) {
                    double horizontalAdvance = 0.0;
                    double horizontalAdvanceNoSpacing = 0.0;
                    double ascent = 0.0;
                    double descent = 0.0;

                    // get painter paths for current text block at height 1.0:
                    getPainterPathsForBlock(
                                "A", QList<QTextLayout::FormatRange>(),
                                horizontalAdvance,
                                horizontalAdvanceNoSpacing,
                                ascent, descent);

                    maxAscent = qMax(maxAscent, ascent * blockHeight.top());
                    minDescent = qMin(minDescent, descent * blockHeight.top());
                }
            }

            // handle stacked text:
            if (stackedText) {
                QRegExp reg;
                reg.setPattern(rxStackedText);
                reg.exactMatch(formatting);

                if (target==PainterPaths) {
                    double horizontalAdvance[2];
                    horizontalAdvance[0] = 0.0;
                    horizontalAdvance[1] = 0.0;
                    double horizontalAdvanceNoSpacing[2];
                    horizontalAdvanceNoSpacing[0] = 0.0;
                    horizontalAdvanceNoSpacing[1] = 0.0;
                    double heightFactor = 0.4;

                    // s==0: superscript, s==1: subscript:
                    for (int s=0; s<=1; ++s) {
                        QString script = reg.cap(s+1);

                        QList<RPainterPath> paths;

                        double ascent = 0.0;
                        double descent = 0.0;

                        QList<QTextLayout::FormatRange> localFormats;

                        QTextLayout::FormatRange fr;
                        fr.start = 0;
                        fr.length = script.length();
                        fr.format = currentFormat.top();
                        localFormats.append(fr);

                        // get painter paths for superscript at height 1.0:
                        paths = getPainterPathsForBlock(
                                    script, localFormats,
                                    horizontalAdvance[s],
                                    horizontalAdvanceNoSpacing[s],
                                    ascent, descent);

                        if (s==0) {
                            maxAscent = qMax(maxAscent, ascent * blockHeight.top() * heightFactor + blockHeight.top()*(1.0-heightFactor));
                        }
                        else {
                            minDescent = qMin(minDescent, descent * blockHeight.top() * heightFactor);
                        }

                        // transform to scale text from 1.0 to current text height * 0.4:
                        QTransform sizeTransform;
                        sizeTransform.scale(blockHeight.top()*heightFactor, blockHeight.top()*heightFactor);

                        // move top text more to the right for italic texts:
                        double xOffset = 0.0;
                        if (s==0 && blockItalic.top()==true) {
                            double y = blockHeight.top()*(1.0-heightFactor);
                            // assume italic means roughly 12 degrees:
                            xOffset = tan(RMath::deg2rad(12)) * y;
                        }

                        // transform for current block due to xCursor position
                        // and top or bottom text position:
                        QTransform blockTransform;
                        blockTransform.translate(xCursor + xOffset,
                                                 s==0 ? blockHeight.top()*(1.0-heightFactor) : 0.0);

                        horizontalAdvance[s] += xOffset / (blockHeight.top() * heightFactor);

                        // combine transforms for current text block:
                        QTransform allTransforms = sizeTransform;
                        allTransforms *= blockTransform;

                        // transform paths of current block and append to paths
                        // of current text line:
                        for (int i=0; i<paths.size(); ++i) {
                            RPainterPath p = paths.at(i);
                            p.transform(allTransforms);
                            linePaths.append(p);
                        }
                    }

                    xCursor += qMax(horizontalAdvance[0], horizontalAdvance[1]) * blockHeight.top() * heightFactor;
                }

                if (target==RichText) {
                    QString super = reg.cap(1);
                    QString sub = reg.cap(2);
                    if (!super.isEmpty()) {
                        richText += QString("<span style=\"vertical-align:super;\">%1</span>").arg(super);
                    }
                    if (!sub.isEmpty()) {
                        richText += QString("<span style=\"vertical-align:sub;\">%1</span>").arg(sub);
                    }
                }
            }

            // prepare for next text block:
            if (lineFeed || paragraphFeed || end) {
                if (!textBlock.isEmpty()) {
                    if (textBlock.at(textBlock.length()-1).isSpace()) {
                        trailingSpaces = true;
                    }
                }
//                else {
//                    if (formatting=="\\~") {
//                        trailingSpaces = true;
//                    }
//                }
            }

            textBlock = "";
            formats.clear();
            QTextLayout::FormatRange fr;
            fr.start = 0;
            fr.length = 0;
            fr.format = currentFormat.top();
            formats.append(fr);

            // handle text line.
            // add all painter paths of the current line to result set of
            // painter paths. apply line feed transformations.
            if (lineFeed || paragraphFeed || end) {
//                qDebug() << "lineFeed: adding text line:";
//                qDebug() << "  maxAscent: " << maxAscent;
//                qDebug() << "  minDescent: " << minDescent;
//                qDebug() << "  minDescentPrevious: " << minDescentPrevious;
//                qDebug() << "got line feed or end";
//                qDebug() << "  trailingSpaces: " << trailingSpaces;

                if (target==RichText) {
                    if (lineFeed || paragraphFeed) {
                        richText += "<br/>";
                    }
                }

                if (lineCounter!=0) {
                    yCursor += (minDescentPrevious - maxAscent) * lineSpacingFactor;
                }

                // calculate line bounding box for alignment without spaces at borders:
                RBox lineBoundingBox;
                for (int i=0; i<linePaths.size(); ++i) {
                    RPainterPath p = linePaths.at(i);
                    lineBoundingBox.growToInclude(p.getBoundingBox());
                }

                double featureSize = lineBoundingBox.getHeight();

                QTransform lineTransform;

                switch (horizontalAlignment) {
                case RS::HAlignAlign:
                case RS::HAlignFit:
                case RS::HAlignLeft:
                    if (!leadingSpaces) {
                        // move completely to the left (left border is 0.0):
                        lineTransform.translate(
                                    -lineBoundingBox.getMinimum().x,
                                    yCursor);
                    }
                    else {
                        lineTransform.translate(0, yCursor);
                    }
                    break;
                case RS::HAlignMid:
                case RS::HAlignCenter:
                    if (!leadingSpaces && !trailingSpaces) {
                        lineTransform.translate(
                                    -(lineBoundingBox.getMinimum().x +
                                      lineBoundingBox.getMaximum().x)/2.0,
                                    yCursor);
                    }
                    else {
                        lineTransform.translate(-xCursor/2.0, yCursor);
                    }
                    break;
                case RS::HAlignRight:
                    if (!trailingSpaces) {
                        lineTransform.translate(-lineBoundingBox.getMaximum().x, yCursor);
                    }
                    else {
                        lineTransform.translate(-xCursor, yCursor);
                    }
                    break;
                }

                width = qMax(width, lineBoundingBox.getMaximum().x - qMin(0.0, lineBoundingBox.getMinimum().x));

                // add current text line to result set:
                QPen pen;
                for (int i=0; i<linePaths.size(); ++i) {
                    RPainterPath p = linePaths[i];
                    if (i==0) {
                        pen = p.getPen();
                        if (pen.style()==Qt::NoPen) {
                           pen = QPen(p.getBrush().color());
                        }
                    }
                    p.transform(lineTransform);
                    p.setFeatureSize(featureSize);
                    painterPaths.append(p);
                    boundingBox.growToInclude(p.getBoundingBox());
                }

                RPainterPath bbPath;
                bbPath.addBox(lineBoundingBox);
                bbPath.transform(lineTransform);
                bbPath.setFeatureSize(-featureSize);
                bbPath.setPen(pen);
                painterPaths.append(bbPath);

                lineCounter++;
                xCursor = 0.0;
                maxAscent = 0.0;
                minDescentPrevious = minDescent;
                minDescent = 0.0;
                linePaths.clear();
                firstBlockInLine = true;
                leadingSpaces = false;
                trailingSpaces = false;
            }
        }

        // handle formatting after text block, that applies either
        // to the next text block(s) or the rest of the current text block:
        if (formatting.isEmpty()) {
            continue;
        }

        QRegExp reg;

        // unicode:
        reg.setPattern(rxUnicode);
        if (reg.exactMatch(formatting)) {
            textBlock += QChar(reg.cap(1).toInt(0, 16));
            continue;
        }

//        // unicode (5 hex digits):
//        reg.setPattern(rxUnicodeM);
//        if (reg.exactMatch(formatting)) {
//            uint code = reg.cap(1).toInt(0, 16);
//            qDebug() << QString("M code: %1").arg(code, 0, 16);
//            QString c = QString::fromUcs4(&code, 1);
//            qDebug() << QString("M char: %1").arg(c);
//            textBlock += c;
//            continue;
//        }

        // curly braket open:
        reg.setPattern(rxCurlyOpen);
        if (reg.exactMatch(formatting)) {
            textBlock += "{";
            continue;
        }

        // curly braket close:
        reg.setPattern(rxCurlyClose);
        if (reg.exactMatch(formatting)) {
            textBlock += "}";
            continue;
        }

        // degree:
        reg.setPattern(rxDegree);
        if (reg.exactMatch(formatting)) {
            textBlock += chDegree;
            continue;
        }

        // plus/minus:
        reg.setPattern(rxPlusMinus);
        if (reg.exactMatch(formatting)) {
            textBlock += chPlusMinus;
            continue;
        }

        // diameter:
        reg.setPattern(rxDiameter);
        if (reg.exactMatch(formatting)) {
            textBlock += chDiameter;
            continue;
        }

        // backslash:
        reg.setPattern(rxBackslash);
        if (reg.exactMatch(formatting)) {
            textBlock += "\\";
            continue;
        }

        // non-breaking space:
        reg.setPattern(rxNonBreakingSpace);
        if (reg.exactMatch(formatting)) {
            if (target==PainterPaths) {
                textBlock += QChar(Qt::Key_nobreakspace);
            }
            if (target==RichText) {
                textBlock += "&nbsp;";
            }
            continue;
        }

        // font change (TTF):
        reg.setPattern(rxFontChangeTtf);
        if (reg.exactMatch(formatting)) {
            blockFont.top() = reg.cap(1);
            blockBold.top() = (reg.cap(2).toInt()!=0);
            blockItalic.top() = (reg.cap(3).toInt()!=0);
            useCadFont.top() = false;
            blockChangedHeightOrFont = true;

            if (target==RichText) {
                QString style;
                style += QString("font-family:%1;").arg(blockFont.top());
                style += QString("font-weight:%1;").arg(blockBold.top() ? "bold" : "normal");
                style += QString("font-style:%1;").arg(blockItalic.top() ? "italic" : "normal");
                richText += QString("<span style=\"%1\">").arg(style);
                openTags.top().append("span");
            }
            continue;
        }

        // font change (CAD):
        reg.setPattern(rxFontChangeCad);
        if (reg.exactMatch(formatting)) {
            blockFont.top() = reg.cap(1);
            useCadFont.top() = true;
            if (xCursor>RS::PointTolerance) {
                RFont* f = RFontList::get(blockFont.top());
                if (f!=NULL && f->isValid()) {
                    xCursor += f->getLetterSpacing() / 9.0;
                }
            }
            blockChangedHeightOrFont = true;

            if (target==RichText) {
                QString style;
                style += QString("font-family:%1;").arg(blockFont.top());
                richText += QString("<span style=\"%1\">").arg(style);
                openTags.top().append("span");
            }
            continue;
        }

        // height change:
        reg.setPattern(rxHeightChange);
        if (reg.exactMatch(formatting)) {
            bool factor = reg.cap(2)=="x";

            if (factor) {
                blockHeight.top() *= reg.cap(1).toDouble();
            }
            else {
                blockHeight.top() = reg.cap(1).toDouble();
            }
            blockChangedHeightOrFont = true;

            if (target==RichText) {
                QString style;
                style += QString("font-size:%1pt;").arg(blockHeight.top() * fontHeightFactor);
                richText += QString("<span style=\"%1\">").arg(style);
                openTags.top().append("span");
            }
            continue;
        }

        QTextLayout::FormatRange fr;
        fr.start = textBlock.length();
        fr.length = 0;

        // start format block:
        reg.setPattern(rxBeginBlock);
        if (reg.exactMatch(formatting)) {
            currentFormat.push(currentFormat.top());
            blockFont.push(blockFont.top());
            blockBold.push(blockBold.top());
            blockItalic.push(blockItalic.top());
            blockHeight.push(blockHeight.top());
            useCadFont.push(useCadFont.top());
            if (target==RichText) {
                openTags.push(QStringList());
            }
            blockChangedHeightOrFont = false;
            continue;
        }

        // end format block:
        reg.setPattern(rxEndBlock);
        if (reg.exactMatch(formatting)) {
            currentFormat.pop();
            fr.format = currentFormat.top();
            formats.append(fr);
            blockFont.pop();
            blockBold.pop();
            blockItalic.pop();
            blockHeight.pop();
            useCadFont.pop();
            blockChangedHeightOrFont = false;

            if (target==RichText) {
                // close all tags that were opened in this block:
                for (int i=openTags.top().size()-1; i>=0; --i) {
                    QString tag = openTags.top().at(i);
                    richText += QString("</%1>").arg(tag);
                }
                openTags.pop();
            }

            continue;
        }

        // color change (indexed):
        reg.setPattern(rxColorChangeIndex);
        if (reg.exactMatch(formatting)) {
            RColor blockColor = RColor::createFromCadIndex(reg.cap(1));
            if (blockColor.isByLayer()) {
                currentFormat.top().setForeground(RColor::CompatByLayer);
            } else if (blockColor.isByBlock()) {
                currentFormat.top().setForeground(RColor::CompatByBlock);
            }
            else {
                currentFormat.top().setForeground(blockColor);
            }
            fr.format = currentFormat.top();
            formats.append(fr);

            if (target==RichText) {
                QString style;
                style += QString("color:%1;").arg(blockColor.name());
                richText += QString("<span style=\"%1\">").arg(style);
                openTags.top().append("span");
            }
            continue;
        }

        // color change (custom):
        reg.setPattern(rxColorChangeCustom);
        if (reg.exactMatch(formatting)) {
            RColor blockColor = RColor::createFromCadCustom(reg.cap(1));
            currentFormat.top().setForeground(blockColor);
            fr.format = currentFormat.top();
            formats.append(fr);

            if (target==RichText) {
                QString style;
                style += QString("color:%1;").arg(blockColor.name());
                richText += QString("<span style=\"%1\">").arg(style);
                openTags.top().append("span");
            }
            continue;
        }
    }

    if (target==PainterPaths) {
        // at this point, the text is at 0/0 with the base line of the
        // first text line at y==0

        // vertical alignment:
        double topLine = qMax(textHeight, boundingBox.getMaximum().y);
        double bottomLine = qMin(0.0, boundingBox.getMinimum().y);

        QTransform globalTransform;
        globalTransform.translate(position.x, position.y);
        globalTransform.rotate(RMath::rad2deg(angle));
        switch (verticalAlignment) {
        case RS::VAlignTop:
            //if (!leadingEmptyLines) {
                globalTransform.translate(0.0, -topLine);
            //}
            break;
        case RS::VAlignMiddle:
            if (leadingEmptyLines || trailingEmptyLines) {
                globalTransform.translate(0.0, -(yCursor+textHeight)/2.0);
            }
            else {
                globalTransform.translate(0.0, -(bottomLine + topLine) / 2.0);
            }
            break;
        case RS::VAlignBottom:
        case RS::VAlignBase:
            if (trailingEmptyLines) {
                globalTransform.translate(0.0, -yCursor);
            }
            else {
                globalTransform.translate(0.0, -bottomLine);
            }
            break;
        }

        height = boundingBox.getHeight();

        // apply global transform for position, angle and vertical alignment:
        boundingBox = RBox();
        for (int i=0; i<painterPaths.size(); ++i) {
            painterPaths[i].transform(globalTransform);
            boundingBox.growToInclude(painterPaths[i].getBoundingBox());
        }
    }

    if (target==RichText) {
        // close all tags that were opened:
        for (int i=openTags.top().size()-1; i>=0; --i) {
            QString tag = openTags.top().at(i);
            richText += QString("</%1>").arg(tag);
        }
    }
}

QList<RPainterPath> RTextRenderer::getPainterPathsForBlock(
    const QString& blockText,
    const QList<QTextLayout::FormatRange>& formats,
    double& horizontalAdvance,
    double& horizontalAdvanceNoSpacing,
    double& ascent,
    double& descent) {

    if (useCadFont.top()) {
        return getPainterPathsForBlockCad(
                    blockText,
                    formats,
                    horizontalAdvance,
                    horizontalAdvanceNoSpacing,
                    ascent, descent);
    }
    else {
        return getPainterPathsForBlockTtf(
                    blockText,
                    formats,
                    horizontalAdvance,
                    horizontalAdvanceNoSpacing,
                    ascent, descent);
    }
}

/**
 * \return Painter paths for the given text block in TTF font,
 *      positioned at 0/0 (bottom left corner) and with a font size of
 *      1 drawing unit.
 */
QList<RPainterPath> RTextRenderer::getPainterPathsForBlockTtf(
    const QString& blockText,
    const QList<QTextLayout::FormatRange>& formats,
    double& horizontalAdvance,
    double& horizontalAdvanceNoSpacing,
    double& ascent,
    double& descent) {

    Q_UNUSED(horizontalAdvanceNoSpacing)

//    qDebug() << "formats: ";
//    for (int i=0; i<formats.length(); i++) {
//        qDebug() << "\tfont: " << formats[i].format.font();
//        qDebug() << "\tpt: " << formats[i].format.fontPointSize();
//        qDebug() << "\ttip: " << formats[i].format.toolTip();
//        qDebug() << "\tstart: " << formats[i].start;
//        qDebug() << "\tlength: " << formats[i].length;
//        qDebug() << "";
//    }

    // leading spaces:
    double leadingSpace = 0.0;

    // spaces are sometimes dropped in Qt 4.8.x (use Qt 4.7.4 instead for now)
//    if (!formats.isEmpty() && blockText.startsWith(' ') && !blockText.trimmed().isEmpty()) {
//        qDebug() << "got leading spaces";
//        QRegExp ex("^[ ]*");
//        if (ex.indexIn(blockText)==0) {
//            double dummy;
//            getPainterPathsForBlockTtf(
//                blockText.left(ex.matchedLength()),
//                formats,
//                leadingSpace,
//                dummy, dummy, dummy);
//            qDebug() << "leading space is: " << leadingSpace;
//        }
//    }

    QFont font(blockFont.top());
    // drawing with a 1pt font will freak out Windows:
    font.setPointSizeF(100.0);
    font.setBold(blockBold.top());
    font.setItalic(blockItalic.top());

    // bounding boxes for 1.0 height font:
    QRectF boxA = getCharacterRect(font, 'A');
    QRectF boxG = getCharacterRect(font, 'g');

    double heightA = boxA.height() * 100.0;

    // scale to scale from 100.0pt font to a font size where an 'A' is
    // exactly 1 unit tall:
    double ttfScale = 1.0 / heightA;
    descent = (boxA.bottom()*100 - boxG.bottom()*100) * ttfScale;
    QFontMetricsF fm(font);
    ascent = fm.ascent() * ttfScale;

    // empty text, e.g. line feed only:
    if (blockText=="") {
        horizontalAdvance = 0.0;
        return QList<RPainterPath>();
    }

    double topSpacing = boxA.y() * 100.0;

    // render text into painter paths using a QTextLayout:
    QTextLayout layout;

    layout.setFont(font);
    layout.setText(blockText);
    layout.setAdditionalFormats(formats);

    layout.beginLayout();
    QTextLine line = layout.createLine();
    if (!line.isValid()) {
        horizontalAdvance = 0.0;
        qWarning("RTextRenderer::getPainterPathsForBlock: got not a single line");
        return QList<RPainterPath>();
    }
    layout.endLayout();

    horizontalAdvance = line.horizontalAdvance() * ttfScale;

    RPainterPathDevice ppd;
    QPainter ppPainter(&ppd);
    layout.draw(&ppPainter, QPoint(0,0));
    ppPainter.end();

    // transform to exactly 1.0 height for an 'A',
    // reference point is bottom left (to make sure that texts of different
    // heights are aligned at the bottom, not top):
    QTransform t;
    t.scale(ttfScale, -ttfScale);
    t.translate(leadingSpace/ttfScale, -topSpacing-heightA);

    QList<RPainterPath> ret;
    QList<RPainterPath> paths = ppd.getPainterPaths();
    for (int i=0; i<paths.size(); ++i) {
        RPainterPath p = paths.at(i);
        p.transform(t);
        ret.append(p);
    }

    return ret;
}

/**
 * \return Painter paths for the given text block in CAD font,
 *      positioned at 0/0 (bottom left corner) and with a font size of
 *      1 drawing unit.
 */
QList<RPainterPath> RTextRenderer::getPainterPathsForBlockCad(
    const QString& blockText,
    const QList<QTextLayout::FormatRange>& formats,
    double& horizontalAdvance,
    double& horizontalAdvanceNoSpacing,
    double& ascent,
    double& descent) {

    QList<RPainterPath> ret;

    RFont* font = RFontList::get(blockFont.top());
    if (font==NULL || !font->isValid()) {
        qWarning() << "RTextRenderer::getPainterPathsForBlockCad: "
                   << "invalid font: " << blockFont.top() << " - using 'standard' instead...";

        // 20120309: resort to standard font (better than nothing):
        font = RFontList::get("standard");
    }

    double cursor = 0.0;
    // cxf fonts define glyphs at a scale of 9:1:
    double cxfScale = 1.0/9.0;
    QColor currentColor;
    bool gotLetterSpacing = false;

    for (int i=0; i<blockText.length(); ++i) {
        QChar ch = blockText.at(i);

        if (ch==' ' || ch==QChar(Qt::Key_nobreakspace)) {
            if (gotLetterSpacing) {
                cursor -= font->getLetterSpacing() * cxfScale;
            }
            cursor += font->getWordSpacing() * cxfScale;
            gotLetterSpacing = false;
            continue;
        }

        // handle color and other formats
        for (int fi=0; fi<formats.size(); ++fi) {
            QTextLayout::FormatRange format = formats.at(fi);

            if (format.start==i && format.start+format.length>i) {
                QColor color = format.format.foreground().color();
                currentColor = color;
            }
        }

        QPainterPath glyph = font->getGlyph(ch, draft);
        // glyph not available in font (show as question mark):
        if (glyph.elementCount()==0) {
            glyph = font->getGlyph('?', draft);
        }
        // if question mark is not available, show nothing:
        if (glyph.elementCount()>0) {
            RPainterPath path(glyph);
            QPen pen = path.getPen();
            pen.setStyle(Qt::SolidLine);
            pen.setColor(currentColor);
            path.setPen(pen);
            if (currentColor.isValid()) {
                // fixed color:
                path.setFixedPenColor(true);
            }

            QTransform transform;
            transform.translate(cursor, 0);
            transform.scale(cxfScale, cxfScale);
            path.transform(transform);
            ret.append(path);

            // letter spacing:
            cursor += path.boundingRect().width() + font->getLetterSpacing() * cxfScale;
            gotLetterSpacing = true;
        }
    }

    horizontalAdvance = cursor;
    horizontalAdvanceNoSpacing = cursor - font->getLetterSpacing() * cxfScale;
    ascent = 1.08;
    descent = -0.36;

    return ret;
}

/**
 * \return Bounding box of the given character with the given font in
 *      size '1', rendered at 0/0. Used to find out the CAD relevant height,
 *      ascent and descent of the font.
 */
QRectF RTextRenderer::getCharacterRect(const QString& fontName, const QChar& ch) const {
    return getCharacterRect(QFont(fontName), ch);
}

/**
 * \overload
 */
QRectF RTextRenderer::getCharacterRect(const QFont& font, const QChar& ch) const {
    QFont font1(font);
    font1.setPointSizeF(100.0);

    QTextLayout layout;
    layout.setFont(font1);
    layout.setText(QString(ch));

    layout.beginLayout();
    layout.createLine();
    layout.endLayout();

    RPainterPathDevice ppd;
    QPainter ppPainter(&ppd);
    layout.draw(&ppPainter, QPoint(0,0));
    ppPainter.end();

    QPainterPath p;
    QList<RPainterPath> paths = ppd.getPainterPaths();
    for (int i=0; i<paths.size(); i++) {
        p.addPath(paths.at(i));
    }

    QRectF rect = p.boundingRect();
    return QRectF(rect.left()/100.0, rect.top()/100.0, rect.width()/100.0, rect.height()/100.0);
}

QString RTextRenderer::getRichTextForBlock(
                            const QString& blockText,
                            const QList<QTextLayout::FormatRange>& formats) {

    Q_UNUSED(formats)

    return Qt::escape(blockText).replace(' ', "&nbsp;");
    //return blockText;
}
