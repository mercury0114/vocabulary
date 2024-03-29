package com.mercury0114.vocabulary;

import static com.google.common.collect.ImmutableList.toImmutableList;
import static com.google.common.collect.MoreCollectors.toOptional;
import static com.mercury0114.vocabulary.QuestionAnswer.extractQuestionAnswer;
import static com.mercury0114.vocabulary.StatisticsEntry.createStatisticsEntry;
import static com.mercury0114.vocabulary.StatisticsEntry.findEntryOrEmptyEntry;
import static java.util.Collections.sort;

import com.google.common.collect.ImmutableList;
import com.mercury0114.vocabulary.QuestionAnswer.AnswerStatus;
import com.mercury0114.vocabulary.QuestionAnswer.Column;
import java.util.ArrayList;
import java.util.Optional;

public class Statistics {

  private final ImmutableList<StatisticsEntry> statisticsEntries;

  Statistics(ImmutableList<StatisticsEntry> statisticsEntries) {
    this.statisticsEntries = statisticsEntries;
  }

  static Statistics createStatisticsFromLines(ImmutableList<String> fileLines) {
    ImmutableList<StatisticsEntry> entries =
        fileLines.stream()
            .map(fileLine -> createStatisticsEntry(fileLine))
            .collect(toImmutableList());
    return new Statistics(entries);
  }

  ImmutableList<String> getHardestQuestions(int requestedNumber) {
    ImmutableList<String> questions =
        statisticsEntries.stream().map(entry -> entry.question()).collect(toImmutableList());
    ImmutableList<String> hardestQuestions =
        sortEntriesHardestFirst().stream()
            .map(entry -> entry.question())
            .collect(toImmutableList());
    return hardestQuestions.subList(0, Math.min(hardestQuestions.size(), requestedNumber));
  }

  private ImmutableList<StatisticsEntry> sortEntriesHardestFirst() {
    ArrayList<StatisticsEntry> entriesToSort = new ArrayList<>(this.statisticsEntries);
    sort(entriesToSort, StatisticsEntry.getComparatorForStatisticsEntry());
    return ImmutableList.copyOf(entriesToSort);
  }

  void updateOneStatisticsEntry(String question, AnswerStatus answerStatus) {
    StatisticsEntry entryMatchingQuestion = findEntry(question).get();
    entryMatchingQuestion.incrementCounter(answerStatus);
  }

  ImmutableList<String> prepareUpdatedStatisticsFileLines(
      Column column,
      ImmutableList<String> currentVocabularyFileLines,
      ImmutableList<String> oldStatisticsFileLines) {
    ImmutableList<String> questions =
        currentVocabularyFileLines.stream()
            .map(line -> extractQuestionAnswer(line, column))
            .map(questionAnswer -> questionAnswer.question)
            .collect(toImmutableList());
    ImmutableList<StatisticsEntry> oldStatisticsEntries =
        oldStatisticsFileLines.stream()
            .map(line -> createStatisticsEntry(line))
            .collect(toImmutableList());
    ImmutableList<StatisticsEntry> upToDateEntries =
        questions.stream()
            .map(question -> returnMostUpToDateStatisticsEntry(question, oldStatisticsEntries))
            .collect(toImmutableList());
    return upToDateEntries.stream()
        .map(entry -> entry.convertToFileLine())
        .collect(toImmutableList());
  }

  private StatisticsEntry returnMostUpToDateStatisticsEntry(
      String question, ImmutableList<StatisticsEntry> oldStatisticsEntries) {
    return findEntry(question).orElse(findEntryOrEmptyEntry(question, oldStatisticsEntries));
  }

  private Optional<StatisticsEntry> findEntry(String question) {
    return this.statisticsEntries.stream()
        .filter(entry -> entry.question().equals(question))
        .collect(toOptional());
  }
}
