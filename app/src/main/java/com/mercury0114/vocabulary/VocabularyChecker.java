package com.mercury0114.vocabulary;

import com.google.common.collect.ImmutableList;
import com.mercury0114.vocabulary.QuestionAnswer.AnswerStatus;
import com.mercury0114.vocabulary.QuestionAnswer.Column;
import java.util.ArrayList;
import java.util.Collections;

public class VocabularyChecker {
  public static class EmptyFileException extends RuntimeException {
    private EmptyFileException(String filePath) {
      super(filePath);
    }
  }

  public static class NoQuestionsException extends RuntimeException {}

  private final ArrayList<QuestionAnswer> questionAnswerList = new ArrayList();
  private final int penaltyFactor;
  private Column column;

  public VocabularyChecker(int penaltyFactor, Column column) {
    if (penaltyFactor <= 0) {
      throw new IllegalArgumentException();
    }
    this.penaltyFactor = penaltyFactor;
    this.column = column;
  }

  public void prepareQuestions(ImmutableList<String> lines) {
    for (String line : lines) {
      String[] words = line.split(", ");
      QuestionAnswer left = new QuestionAnswer(words[0], words[1]);
      QuestionAnswer right = new QuestionAnswer(words[1], words[0]);
      for (int i = 0; i < penaltyFactor; i++) {
        switch (column) {
          case LEFT:
            questionAnswerList.add(left);
            break;
          case RIGHT:
            questionAnswerList.add(right);
            break;
          case BOTH:
            questionAnswerList.add(left);
            questionAnswerList.add(right);
            break;
          default:
            throw new RuntimeException("Wrong Column enum value");
        }
      }
    }
    Collections.shuffle(questionAnswerList);
  }

  public AnswerStatus checkAnswer(String answer) {
    QuestionAnswer questionAnswer = questionAnswerList.get(0);
    AnswerStatus answerStatus = questionAnswer.getAnswerStatus(answer);
    switch (answerStatus) {
      case CORRECT:
        questionAnswerList.remove(0);
        Collections.shuffle(questionAnswerList);
        putDifferentQuestionFirst(questionAnswer);
        break;
      case CLOSE:
        break;
      case WRONG:
        updateQuestionAnswerList(1);
        break;
    }
    return answerStatus;
  }

  public int questionsRemaining() {
    return questionAnswerList.size();
  }

  public String nextQuestion() throws NoQuestionsException {
    if (questionAnswerList.size() == 0) {
      throw new NoQuestionsException();
    }
    return questionAnswerList.get(0).question;
  }

  public String revealAnswer() {
    QuestionAnswer questionAnswer = questionAnswerList.get(0);
    updateQuestionAnswerList(penaltyFactor);
    putDifferentQuestionFirst(questionAnswer);
    return questionAnswer.answer;
  }

  private void putDifferentQuestionFirst(QuestionAnswer previous) {
    for (int i = 0; i < questionAnswerList.size(); i++) {
      if (!questionAnswerList.get(i).same(previous)) {
        Collections.swap(questionAnswerList, 0, i);
      }
    }
  }

  private void updateQuestionAnswerList(int penaltyFactor) {
    QuestionAnswer questionAnswer = questionAnswerList.get(0);
    for (int i = 0; i < penaltyFactor; i++) {
      questionAnswerList.add(questionAnswer);
    }
  }
}
